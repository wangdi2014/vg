#ifndef INDEX_H
#define INDEX_H

#include <iostream>
#include <exception>
#include "leveldb/db.h"
#include "vg.h"

using namespace std;

namespace vg {

class Index : public leveldb::DB {

public:

    Index(string& name);
    ~Index(void);

    leveldb::DB* db;
    leveldb::Options options;

    // or should it be "index_graph" ?
    // we're storing it in the leveldb
    void load_graph(VariantGraph& graph);

    void index_kmers(VariantGraph& graph, int kmer_size = 15);
    void index_positions(VariantGraph& graph, map<long, Node*>& node_path, map<long, Edge*>& edge_path);

    // once we have indexed the kmers, we can get the nodes and edges matching
    void kmer_matches(string& kmer, set<int64_t>& node_ids, set<int64_t>& edge_ids);

};

class indexOpenException: public exception
{
    virtual const char* what() const throw()
    {
        return "unable to open variant graph index";
    }
} indexOpenException;

}

#endif
