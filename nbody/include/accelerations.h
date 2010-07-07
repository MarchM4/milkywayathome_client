/* Copyright 2010 Matthew Arsenault, Travis Desell, Boleslaw
Szymanski, Heidi Newberg, Carlos Varela, Malik Magdon-Ismail and
Rensselaer Polytechnic Institute.

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

#ifndef _ACCELERATIONS_H_
#define _ACCELERATIONS_H_

#include "nbody_types.h"

void sphericalAccel(vectorptr restrict acc, const Spherical* sph, const vectorptr restrict pos);
void miyamotoNagaiDiskAccel(vectorptr restrict acc, const Disk* d, const vectorptr restrict pos);
void exponentialDiskAccel(vectorptr restrict acc, const Disk* d, const vectorptr restrict pos);
void triaxialHaloAccel(vectorptr restrict acc, const Halo* h, const vectorptr restrict pos);
void logHaloAccel(vectorptr restrict acc, const Halo* h, const vectorptr restrict pos);
void nfwHaloAccel(vectorptr restrict acc, const Halo* h, const vectorptr restrict pos);

#endif /* _ACCELERATIONS_H_ */
