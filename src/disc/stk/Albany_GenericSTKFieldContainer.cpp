//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Albany_GenericSTKFieldContainer.hpp"
#include "Albany_GenericSTKFieldContainer_Def.hpp"

namespace Albany {

template class GenericSTKFieldContainer<true>;
template class GenericSTKFieldContainer<false>;

} // namespace Albany
