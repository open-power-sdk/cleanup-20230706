/*
 Copyright (c) [2019] Steven Munroe.

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 vec_runtime_PWR9.c

 Contributors:
      Steven Munroe
      Created on: Aug 20, 2019
 */

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
/* Not LITTLE Endian, lets call the whole thing off.
   Older distros running Big Endian are unlikely to support PWR9.  */
#pragma GCC target ("cpu=power9")

#include "vec_int512_runtime.c"
#endif


