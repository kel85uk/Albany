//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef ALBANY_DISCRETIZATION_UTILS_HPP
#define ALBANY_DISCRETIZATION_UTILS_HPP

#include <vector>
#include <string>
#include <map>

#include "Teuchos_ArrayRCP.hpp"

#include "Albany_ScalarOrdinalTypes.hpp"
#include "Albany_KokkosTypes.hpp"

namespace AAdapt { namespace rc { class Manager; } }

namespace Albany {

typedef std::map<std::string, std::vector<std::vector<int> > > NodeSetList;
typedef std::map<std::string, std::vector<GO> > NodeSetGIDsList;
typedef std::map<std::string, std::vector<double*> > NodeSetCoordList;

class SideStruct {

  public:

    GO side_GID; // the global id of the side in the mesh
    GO elem_GID; // the global id of the element containing the side
    int elem_LID; // the local id of the element containing the side
    int elem_ebIndex; // The index of the element block that contains the element
    unsigned side_local_id; // The local id of the side relative to the owning element

};

typedef std::map<std::string, std::vector<SideStruct> > SideSetList;

class wsLid {

  public:

    int ws; // the workset of the element containing the side
    int LID; // the local id of the element containing the side

};

typedef std::map<GO, wsLid > WsLIDList;

template <typename T>
struct WorksetArray {
   typedef Teuchos::ArrayRCP<T> type;
};

// LB 8/17/18: I moved these out of AbstractDiscretization, so if one only needs these types,
//             he/she can include this small file rather than Albany_AbstractDiscretization.hpp,
//             which has tons of dependencies.
using WorksetConn = Kokkos::View<LO***, Kokkos::LayoutRight, PHX::Device>;
using Conn        = WorksetArray<WorksetConn>::type;

} // namespace Albany

#endif // ALBANY_DISCRETIZATION_UTILS_HPP
