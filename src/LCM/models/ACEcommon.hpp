//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#if !defined(LCM_ACEcommon_hpp)
#define LCM_ACEcommon_hpp

#include "Albany_Utils.hpp"

namespace LCM {

std::vector<RealType>
vectorFromFile(std::string const& filename);

}  // namespace LCM

#endif  // LCM_ACEcommon_hpp
