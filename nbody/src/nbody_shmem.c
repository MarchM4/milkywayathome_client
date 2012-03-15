/*
Copyright (c) 2011 Matthew Arsenault
Copyright (c) 2011 Rensselaer Polytechnic Institute.

This file is part of Milkway@Home.

Milkyway@Home is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Milkyway@Home is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Milkyway@Home.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "nbody.h"
#include "nbody_priv.h"
#include "milkyway_util.h"
#include "nbody_grav.h"
#include "nbody_show.h"
#include "nbody_lua.h"
#include "nbody_shmem.h"
#include "nbody_defaults.h"

#if USE_SHMEM
  #include <sys/mman.h>
  #include <sys/stat.h>
  #include <fcntl.h>
  #include <errno.h>
  #include <err.h>
#endif


/* Not actually necessary, but checking the next available one too
 * many times will take forever */
#define MAX_INSTANCES 128


static const char nbodyGraphicsName[] = NBODY_GRAPHICS_NAME;

static void nbPrepareSceneFromState(const NBodyCtx* ctx, const NBodyState* st)
{
    st->scene->nbodyMajorVersion = NBODY_VERSION_MAJOR;
    st->scene->nbodyMinorVersion = NBODY_VERSION_MINOR;
    st->scene->nbody = st->nbody;
    st->scene->hasGalaxy = (ctx->potentialType == EXTERNAL_POTENTIAL_DEFAULT);
}

static size_t nbFindShmemSize(int nbody)
{
    size_t snapshotSize = sizeof(NBodyCircularQueue) + nbody * sizeof(FloatPos);
    return sizeof(scene_t) + NBODY_CIRC_QUEUE_SIZE * snapshotSize;
}

#if USE_SHMEM

/* Create the next available segment of the form /milkyway_nbody_n n = 0 .. 127*/
int nbCreateSharedScene(NBodyState* st, const NBodyCtx* ctx)
{
    size_t size = nbFindShmemSize(st->nbody);
    int shmId = -1;
    const int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    void* p = NULL;
    int instanceId = -1;
    char name[128];

    /* Try looking for the next available segment of the form /milkyway_nbody_<n> */
    while (shmId < 0 && instanceId < MAX_INSTANCES)
    {
        ++instanceId;

        if (snprintf(name, sizeof(name), "/milkyway_nbody_%d", instanceId) == sizeof(name))
            mw_panic("Buffer too small for shared memory name\n");

        shmId = shm_open(name, O_CREAT | O_RDWR | O_EXCL, mode); /* Try to open exclusively */
        if (shmId < 0 && errno != EEXIST) /* Only failed if */
        {
            mwPerror("Error creating shared memory '%s'", name);
            return 1;
        }
    }

    if (instanceId >= MAX_INSTANCES)
    {
        mw_printf("Could not open new shm segment in %d tries\n", MAX_INSTANCES);
        return 1;
    }

    if (ftruncate(shmId, size) < 0) /* Make the segment the correct size */
    {
        mwPerror("Error ftruncate() shared memory");
        if (shm_unlink(name) < 0)
        {
            mwPerror("Error unlinking shared memory '%s'", name);
        }

        return 1;
    }

    p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shmId, 0);
    if (p == MAP_FAILED)
    {
        mwPerror("mmap: Failed to mmap shared memory");
        if (shm_unlink(name) < 0)
        {
            mwPerror("Error unlinking shared memory '%s'", name);
        }

        return 1;
    }

    st->scene = (scene_t*) p;
    st->shmId = shmId;
    st->scene->instanceId = instanceId;
    strncpy(st->scene->shmemName, name, sizeof(st->scene->shmemName));
    nbPrepareSceneFromState(ctx, st);

    return 0;
}

#elif USE_BOINC_SHMEM

int nbCreateSharedScene(NBodyState* st, const NBodyCtx* ctx)
{
    size_t size = nbFindShmemSize(st->nbody);

    st->scene = (scene_t*) mw_graphics_make_shmem(NBODY_BIN_NAME, (int) size);
    if (!st->scene)
    {
        mw_printf("Failed to get shmem of size %d\n", (int) size);
        return 1;
    }

    memset(st->scene, 0, size);
    nbPrepareSceneFromState(ctx, st);

    return 0;
}

#else

int nbCreateSharedScene(NBodyState* st, const NBodyCtx* ctx)
{
    (void) st, (void) ctx;
    mw_printf("Creating shared scene unimplemented for this system\n");
    return 0;
}

int visualizerIsAttached(const NBodyState* st)
{
    (void) st;
    return 0;
}

#endif /* USE_SHMEM */


#ifndef _WIN32

void nbLaunchVisualizer(NBodyState* st, const char* visArgs)
{
    pid_t pid;
    const char* path = NULL;
    char* newPath = NULL;
    int argc = 0;
    char* buf = NULL;
    char* p = NULL;
    char** argv = NULL;
    size_t argvSize = 0;
    size_t visArgsLen = 0;
    char idArg[128];

    if (!st->scene) /* If there's no scene to share, there's no point */
        return;

    if (st->usesExact)
    {
        mw_printf("Visualizer broken with Exact\n");
        return;
    }

    pid = fork();
    if (pid != 0)  /* Parent */
        return;

    /* Child */

    /* Put places convenient for testing. Not essential, failure of
     * any of these is OK */
    path = getenv("PATH");
    if (!path)
    {
        mwPerror("Error getting PATH");
    }
    else
    {
        if (asprintf(&newPath, ".:../bin/:%s", path) < 0)
        {
            mwPerror("Appending to path");
        }
        else
        {
            if (setenv("PATH", newPath, TRUE) < 0)
            {
                mwPerror("Error setting PATH");
            }
            free(newPath);
        }
    }

    if (snprintf(idArg, sizeof(idArg), "--instance-id=%d ", st->scene->instanceId) == sizeof(idArg))
        mw_panic("Buffer too small for --instance-id visualizer argument\n");

    /* Stick the program name at the head of the arguments passed in */
    visArgsLen = visArgs ? strlen(visArgs) : 0;
    argvSize = visArgsLen + sizeof(idArg) + sizeof(nbodyGraphicsName) + 2; /* arguments + program name + space + null */
    buf = mwCalloc(argvSize, sizeof(char));

    p = stpcpy(buf, nbodyGraphicsName);
    p = stpcpy(p, " ");
    p = stpcpy(p, idArg);
    if (visArgs)
    {
        stpcpy(p, visArgs);
    }

    if (poptParseArgvString(buf, &argc, (const char***) &argv))
    {
        mw_printf("Error parsing arguments for visualizer '%s'\n", visArgs);
        free(buf);
        return;
    }

    if (execvp(argv[0], argv) < 0)
    {
        mwPerror("Failed to launch visualizer '%s'", argv[0]);
    }

    free(buf);
    free(argv);
    mw_finish(EXIT_SUCCESS);  /* Unnecessary */
}

#else

void nbLaunchVisualizer(NBodyState* st, const char* visArgs)
{
    PROCESS_INFORMATION pInfo;
    STARTUPINFO startInfo;
    size_t visArgsLen, argvSize;
    char* buf;

    (void) st;

    memset(&pInfo, 0, sizeof(pInfo));
    memset(&startInfo, 0, sizeof(startInfo));
    startInfo.cb = sizeof(startInfo);

    visArgsLen = visArgs ? strlen(visArgs) : 0;
    argvSize = visArgsLen + sizeof(nbodyGraphicsName) + 2; /* arguments + program name + space + null */
    buf = mwCalloc(argvSize, sizeof(char));

    strcat(buf, nbodyGraphicsName);
    strcat(buf, " ");
    if (visArgs)
    {
        strcat(buf, visArgs);
    }

    if (!CreateProcess(NULL,
                       buf,
                       NULL,
                       NULL,
                       FALSE,
                       NORMAL_PRIORITY_CLASS,
                       NULL,
                       NULL,
                       &startInfo,
                       &pInfo))
    {
        mw_printf("Error creating visualizer process: %ld\n", GetLastError());
    }

    free(buf);
}

#endif /* _WIN32 */

static void nbWriteSnapshot(NBodyCircularQueue* queue, int buffer, const NBodyCtx* ctx, const NBodyState* st)
{
    int i;
    const Body* b;
    mwvector cmPos;
    int nbody = st->nbody;
    SceneInfo* info = &queue->info[buffer];
    FloatPos* r = &queue->bodyData[buffer * nbody];

    if (st->usesExact) /* Need to find it since we don't have the tree */
    {
        /* TODO: This should use parallel reduction */
        cmPos = nbCenterOfMass(st);
    }
    else
    {
        cmPos = Pos(st->tree.root);
    }

    info->currentTime = (float) st->step * (float) ctx->timestep;
    info->timeEvolve = (float) ctx->timeEvolve;

    // FIXME: CM when using exact
    info->rootCenterOfMass[0] = (float) X(cmPos);
    info->rootCenterOfMass[1] = (float) Y(cmPos);
    info->rootCenterOfMass[2] = (float) Z(cmPos);

/*
    if (FALSE)
    {
        // Tell the graphics about the orbit's history
        int i = scene->currentTracePoint;

        scene->rootCenterOfMass[0] = (float) X(cmPos);
        scene->rootCenterOfMass[1] = (float) Y(cmPos);
        scene->rootCenterOfMass[2] = (float) Z(cmPos);

        if (i < N_ORBIT_TRACE_POINTS && i < MAX_DRAW_TRACE_POINTS)
        {
            if (X(st->orbitTrace[i]) < REAL_MAX)
            {
                scene->orbitTrace[i].x = (float) X(st->orbitTrace[i]);
                scene->orbitTrace[i].y = (float) Y(st->orbitTrace[i]);
                scene->orbitTrace[i].z = (float) Z(st->orbitTrace[i]);

                scene->currentTracePoint++;
            }
        }
    }
*/

  #ifdef _OPENMP
    #pragma omp parallel for private(i, b) schedule(static)
  #endif
    for (i = 0; i < nbody; ++i)
    {
        b = &st->bodytab[i];
        r[i].x = (float) X(Pos(b));
        r[i].y = (float) Y(Pos(b));
        r[i].z = (float) Z(Pos(b));
        r[i].ignore = ignoreBody(b);
    }
}

static int nbPushCircularQueue(NBodyCircularQueue* queue, const NBodyCtx* ctx, const NBodyState* st)
{
    int head, tail, nextTail;
    tail = OPA_load_int(&queue->tail);
    head = OPA_load_int(&queue->head);

    nextTail = (tail + 1) % NBODY_CIRC_QUEUE_SIZE;
    if (nextTail != head)
    {
        nbWriteSnapshot(queue, tail, ctx, st);
        OPA_store_int(&queue->tail, nextTail);
        return TRUE;
    }
    else /* write failed, queue full */
    {
        return FALSE;
    }
}

void nbUpdateDisplayedBodies(const NBodyCtx* ctx, NBodyState* st)
{
    scene_t* scene = st->scene;

    if (!scene)
        return;

    /* No copying when no screensaver attached */
    if (OPA_load_int(&scene->attachedPID) != 0)
    {
        if (OPA_load_int(&scene->blockSimulationOnGraphics))
        {
            double startTime = mwGetTime();
            double dt;
            int pid;

            /* FIXME: This needs to change if we allow changing
             * whether the simulation should block
             *
             * FIXME: do we trust the graphics process to clean up the
             * attachedPID?
             */

            while (   !nbPushCircularQueue(&scene->queue, ctx, st)
                   && ((pid = OPA_load_int(&scene->attachedPID)) != 0)
                   && (dt = mwGetTime() - startTime) < NBODY_QUEUE_TIMEOUT)
            {
                mwMilliSleep(NBODY_QUEUE_SLEEP_INTERVAL);
            }

            if (dt >= NBODY_QUEUE_TIMEOUT)
            {
                mw_printf("Blocking on graphics timed out\n");
            }

            if (pid == 0)
            {
                mw_printf("Graphics process quit while waiting\n");
            }
        }
        else
        {
            nbPushCircularQueue(&scene->queue, ctx, st);
        }
    }
}

