#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include <cgnslib.h>
#include <metis.h>

int main(int argc, char *argv[])
{
    using namespace std;

    if(argc != 3)
    {
        cerr << "Usage: partcgns filename nparts\n";
        exit(-1);
    }

    const string filename = argv[1];
    char name[256];

    cout << "Reading file \"" << filename << "\"...\n";

    //- Open the file. Assume only one base and one zone
    int fileId, baseId = 1, zoneId = 1;
    cg_open(filename.c_str(), CG_MODE_READ, &fileId);

    cgsize_t sizes[2], one = 1;

    //- Read the zone info
    cg_zone_read(fileId, baseId, zoneId, name, sizes);

    cout << "Zone \"" << name << "\" with " << sizes[0] << " nodes and " << sizes[1] << " elements.\n";

    //- Read in the node coordinates
    vector<double> xCoords(sizes[0]), yCoords(sizes[0]);

    cg_coord_read(fileId, baseId, zoneId, "CoordinateX", RealDouble, &one, &sizes[0], xCoords.data());
    cg_coord_read(fileId, baseId, zoneId, "CoordinateY", RealDouble, &one, &sizes[0], yCoords.data());

    //- Section data
    int nSections, secNo = 1;
    cg_nsections(fileId, baseId, zoneId, &nSections);

    ElementType_t type;
    int nBoundary, parentFlag;
    cgsize_t eBeg, eEnd;

    cg_section_read(fileId, baseId, zoneId, secNo, name, &type, &eBeg, &eEnd, &nBoundary, &parentFlag);

    if(type != TRI_3)
    {
        cerr << "This example only works for triangular meshes.\n";
        exit(-1);
    }

    vector<cgsize_t> elems(3*sizes[1]);
    cg_elements_read(fileId, baseId, zoneId, secNo, elems.data(), NULL);

    cg_close(fileId);

    //- Finished reading in mesh data. Now, partition the mesh

    //    int METIS PartMeshDual(idx t *ne, idx t *nn, idx t *eptr, idx t *eind, idx t *vwgt, idx t *vsize,
    //    idx t *ncommon, idx t *nparts, real t *tpwgts, idx t *options, idx t *objval,
    //    idx t *epart, idx t *npart)

    idx_t ne = sizes[1];
    idx_t nn = sizes[0];
    idx_t nparts = atoi(argv[2]);
    idx_t ncommon = 2;
    idx_t objval;

    vector<idx_t> eind(3*ne);
    vector<idx_t> eptr(ne + 1);
    vector<idx_t> epart(ne);
    vector<idx_t> npart(nn);

    cout << "Partitioning mesh into " << nparts << " partitions...\n";

    eptr[0] = 0;
    for(int i = 0; i < ne; ++i)
    {
        eind[3*i] = elems[3*i] - 1;
        eind[3*i + 1] = elems[3*i + 1] - 1;
        eind[3*i + 2] = elems[3*i + 2] - 1;

        eptr[i + 1] = 3*(i + 1);
    }


    int status = METIS_PartMeshDual(&ne, &nn, eptr.data(), eind.data(), NULL, NULL, &ncommon, &nparts, NULL, NULL, &objval, epart.data(), npart.data());

    //- Now, write the partitioned mesh to a cgns file!

    cg_open(("partitioned_" + filename).c_str(), CG_MODE_WRITE, &fileId);
    cg_base_write(fileId, "mesh", 2, 2, &baseId);
    cg_zone_write(fileId, baseId, "Zone 1", sizes, Unstructured, &zoneId);

    int xid;
    cg_coord_write(fileId, baseId, zoneId, RealDouble, "CoordinateX", xCoords.data(), &xid);
    cg_coord_write(fileId, baseId, zoneId, RealDouble, "CoordinateY", yCoords.data(), &xid);

    cgsize_t start = 1, end;
    for(int partNo = 0; partNo < nparts; ++partNo)
    {
        vector<cgsize_t> localElems;

        for(int i = 0; i < ne; ++i)
        {
            if(epart[i] == partNo)
            {
                localElems.push_back(elems[3*i]);
                localElems.push_back(elems[3*i + 1]);
                localElems.push_back(elems[3*i + 2]);
            }
        }

        end = start + localElems.size()/3 - 1;

        int secId;

        char buff[32];
        sprintf(buff, "%d", partNo);
        string secName = string("Proc") + buff;

        cg_section_write(fileId, baseId, zoneId, secName.c_str(), TRI_3, start, end, 0, localElems.data(), &secId);

        start = end + 1;
    }

    cg_close(fileId);
}
