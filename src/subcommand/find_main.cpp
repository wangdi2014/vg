#include "subcommand.hpp"
#include "../vg.hpp"
#include "../utility.hpp"
#include "../mapper.hpp"
#include "../stream/stream.hpp"
#include "../stream/vpkg.hpp"
#include "../region.hpp"
#include "../stream_index.hpp"
#include "../algorithms/sorted_id_ranges.hpp"

#include <unistd.h>
#include <getopt.h>

using namespace vg;
using namespace vg::subcommand;

void help_find(char** argv) {
    cerr << "usage: " << argv[0] << " find [options] >sub.vg" << endl
         << "options:" << endl
         << "    -d, --db-name DIR      use this db (defaults to <graph>.index/)" << endl
         << "    -x, --xg-name FILE     use this xg index (instead of rocksdb db)" << endl
         << "graph features:" << endl
         << "    -n, --node ID          find node(s), return 1-hop context as graph" << endl
         << "    -N, --node-list FILE   a white space or line delimited list of nodes to collect" << endl
         << "    -e, --edges-end ID     return edges on end of node with ID" << endl
         << "    -s, --edges-start ID   return edges on start of node with ID" << endl
         << "    -c, --context STEPS    expand the context of the subgraph this many steps" << endl
         << "    -L, --use-length       treat STEPS in -c or M in -r as a length in bases" << endl
         << "    -p, --path TARGET      find the node(s) in the specified path range(s) TARGET=path[:pos1[-pos2]]" << endl
         << "    -P, --position-in PATH find the position of the node (specified by -n) in the given path" << endl
         << "    -R, --rank-in PATH     find the rank of the node (specified by -n) in the given path" << endl
         << "    -I, --list-paths       write out the path names in the index" << endl
         << "    -X, --approx-pos ID    get the approximate position of this node" << endl
         << "    -r, --node-range N:M   get nodes from N to M" << endl
         << "    -G, --gam GAM          accumulate the graph touched by the alignments in the GAM" << endl
         << "alignments:" << endl
         << "    -d, --db-name DIR      use this RocksDB database to retrieve alignments" << endl
         << "    -l, --sorted-gam FILE  use this sorted, indexed GAM file" << endl
         << "    -a, --alignments       write all alignments from input sorted GAM or RocksDB" << endl
         << "    -o, --alns-on N:M      write alignments which align to any of the nodes between N and M (inclusive)" << endl
         << "    -A, --to-graph VG      get alignments to the provided subgraph" << endl
         << "sequences:" << endl
         << "    -g, --gcsa FILE        use this GCSA2 index of the sequence space of the graph" << endl
         << "    -z, --kmer-size N      split up --sequence into kmers of size N" << endl
         << "    -j, --kmer-stride N    step distance between succesive kmers in sequence (default 1)" << endl
         << "    -S, --sequence STR     search for sequence STR using --kmer-size kmers" << endl
         << "    -M, --mems STR         describe the super-maximal exact matches of the STR (gcsa2) in JSON" << endl
         << "    -B, --reseed-length N  find non-super-maximal MEMs inside SMEMs of length at least N" << endl
         << "    -f, --fast-reseed      use fast SMEM reseeding algorithm" << endl
         << "    -Y, --max-mem N        the maximum length of the MEM (default: GCSA2 order)" << endl
         << "    -Z, --min-mem N        the minimum length of the MEM (default: 1)" << endl
         << "    -k, --kmer STR         return a graph of edges and nodes matching this kmer" << endl
         << "    -T, --table            instead of a graph, return a table of kmers" << endl
         << "                           (works only with kmers in the index)" << endl
         << "    -C, --kmer-count       report approximate count of kmer (-k) in db" << endl
         << "    -D, --distance         return distance on path between pair of nodes (-n). if -P not used, best path chosen heurstically" << endl
         << "haplotypes:" << endl
         << "    -H, --haplotypes FILE  count xg threads in agreement with alignments in the GAM" << endl
         << "    -t, --extract-threads  extract the threads, writing them as paths to the .vg stream on stdout" << endl
         << "    -q, --threads-named S  return all threads whose names are prefixed with string S (multiple allowed)" << endl
         << "    -Q, --paths-named S    return all paths whose names are prefixed with S (multiple allowed)" << endl;

}

int main_find(int argc, char** argv) {

    if (argc == 2) {
        help_find(argv);
        return 1;
    }

    string db_name;
    string sequence;
    int kmer_size=0;
    int kmer_stride = 1;
    vector<string> kmers;
    vector<vg::id_t> node_ids;
    string node_list_file;
    int context_size=0;
    bool use_length = false;
    bool count_kmers = false;
    bool kmer_table = false;
    vector<string> targets;
    string path_name;
    bool position_in = false;
    bool rank_in = false;
    string range;
    string gcsa_in;
    string xg_name;
    bool get_mems = false;
    int mem_reseed_length = 0;
    bool use_fast_reseed = true;
    string sorted_gam_name;
    bool get_alignments = false;
    bool get_mappings = false;
    string aln_on_id_range;
    vg::id_t start_id = 0;
    vg::id_t end_id = 0;
    bool pairwise_distance = false;
    string haplotype_alignments;
    string gam_file;
    int max_mem_length = 0;
    int min_mem_length = 1;
    string to_graph_file;
    bool extract_threads = false;
    vector<string> extract_thread_patterns;
    bool extract_paths = false;
    vector<string> extract_path_patterns;
    vg::id_t approx_id = 0;
    bool list_path_names = false;

    int c;
    optind = 2; // force optind past command positional argument
    while (true) {
        static struct option long_options[] =
            {
                //{"verbose", no_argument,       &verbose_flag, 1},
                {"db-name", required_argument, 0, 'd'},
                {"xg-name", required_argument, 0, 'x'},
                {"gcsa", required_argument, 0, 'g'},
                {"node", required_argument, 0, 'n'},
                {"node-list", required_argument, 0, 'N'},
                {"edges-end", required_argument, 0, 'e'},
                {"edges-start", required_argument, 0, 's'},
                {"kmer", required_argument, 0, 'k'},
                {"table", no_argument, 0, 'T'},
                {"sequence", required_argument, 0, 'S'},
                {"mems", required_argument, 0, 'M'},
                {"reseed-length", required_argument, 0, 'B'},
                {"fast-reseed", no_argument, 0, 'f'},
                {"kmer-stride", required_argument, 0, 'j'},
                {"kmer-size", required_argument, 0, 'z'},
                {"context", required_argument, 0, 'c'},
                {"use-length", no_argument, 0, 'L'},
                {"kmer-count", no_argument, 0, 'C'},
                {"path", required_argument, 0, 'p'},
                {"position-in", required_argument, 0, 'P'},
                {"rank-in", required_argument, 0, 'R'},
                {"node-range", required_argument, 0, 'r'},
                {"sorted-gam", required_argument, 0, 'l'},
                {"alignments", no_argument, 0, 'a'},
                {"mappings", no_argument, 0, 'm'},
                {"alns-on", required_argument, 0, 'o'},
                {"distance", no_argument, 0, 'D'},
                {"haplotypes", required_argument, 0, 'H'},
                {"gam", required_argument, 0, 'G'},
                {"to-graph", required_argument, 0, 'A'},
                {"max-mem", required_argument, 0, 'Y'},
                {"min-mem", required_argument, 0, 'Z'},
                {"extract-threads", no_argument, 0, 't'},
                {"threads-named", required_argument, 0, 'q'},
                {"paths-named", required_argument, 0, 'Q'},
                {"approx-pos", required_argument, 0, 'X'},
                {"list-paths", no_argument, 0, 'I'},
                {0, 0, 0, 0}
            };

        int option_index = 0;
        c = getopt_long (argc, argv, "d:x:n:e:s:o:k:hc:LS:z:j:CTp:P:r:l:amg:M:R:B:fDH:G:N:A:Y:Z:tq:X:IQ:",
                         long_options, &option_index);

        // Detect the end of the options.
        if (c == -1)
            break;

        switch (c)
        {
        case 'd':
            db_name = optarg;
            break;

        case 'x':
            xg_name = optarg;
            break;

        case 'g':
            gcsa_in = optarg;
            break;

        case 'k':
            kmers.push_back(optarg);
            break;

        case 'S':
            sequence = optarg;
            break;

        case 'M':
            sequence = optarg;
            get_mems = true;
            break;
            
        case 'B':
            mem_reseed_length = parse<int>(optarg);
            break;
            
        case 'f':
            use_fast_reseed = true;
            break;

        case 'Y':
            max_mem_length = parse<int>(optarg);
            break;
            
        case 'Z':
            min_mem_length = parse<int>(optarg);
            break;
            
        case 'j':
            kmer_stride = parse<int>(optarg);
            break;

        case 'z':
            kmer_size = parse<int>(optarg);
            break;

        case 'C':
            count_kmers = true;
            break;

        case 'p':
            targets.push_back(optarg);
            break;

        case 'P':
            path_name = optarg;
            position_in = true;
            break;

        case 'R':
            path_name = optarg;
            rank_in = true;
            break;

        case 'c':
            context_size = parse<int>(optarg);
            break;

        case 'L':
            use_length = true;
            break;

        case 'n':
            node_ids.push_back(parse<int>(optarg));
            break;

        case 'N':
            node_list_file = optarg;
            break;

        case 'e':
            end_id = parse<int>(optarg);
            break;

        case 's':
            start_id = parse<int>(optarg);
            break;

        case 'T':
            kmer_table = true;
            break;

        case 'r':
            range = optarg;
            break;
        
        case 'l':
            sorted_gam_name = optarg;
            break;
        
        case 'a':
            get_alignments = true;
            break;

        case 'I':
            list_path_names = true;
            break;

        case 'm':
            get_mappings = true;
            break;

        case 'o':
            aln_on_id_range = optarg;
            break;

        case 'D':
            pairwise_distance = true;
            break;

        case 'H':
            haplotype_alignments = optarg;
            break;

        case 't':
            extract_threads = true;
            break;

        case 'q':
            extract_threads = true;
            extract_thread_patterns.push_back(optarg);
            break;

        case 'Q':
            extract_paths = true;
            extract_path_patterns.push_back(optarg);
            break;

        case 'X':
            approx_id = parse<int>(optarg);
            break;

        case 'G':
            gam_file = optarg;
            break;

        case 'A':
            to_graph_file = optarg;
            break;

        case 'h':
        case '?':
            help_find(argv);
            exit(1);
            break;

        default:
            abort ();
        }
    }
    if (optind < argc) {
        cerr << "[vg find] find does not accept positional arguments" << endl;
        return 1;
    }

    if (db_name.empty() && gcsa_in.empty() && xg_name.empty() && sorted_gam_name.empty()) {
        cerr << "[vg find] find requires -d, -g, -x, or -l to know where to find its database" << endl;
        return 1;
    }

    if (context_size > 0 && use_length == true && xg_name.empty()) {
        cerr << "[vg find] error, -L not supported without -x" << endl;
        exit(1);
    }
    
    if (xg_name.empty() && mem_reseed_length) {
        cerr << "error:[vg find] SMEM reseeding requires an XG index. Provide XG index with -x." << endl;
        exit(1);
    }
    
    // process input node list
    if (!node_list_file.empty()) {
        ifstream nli;
        nli.open(node_list_file);
        if (!nli.good()){
            cerr << "[vg find] error, unable to open the node list input file." << endl;
            exit(1);
        }
        string line;
        while (getline(nli, line)){
            for (auto& idstr : split_delims(line, " \t")) {
                node_ids.push_back(parse<int64_t>(idstr.c_str()));
            }
        }
        nli.close();
    }

    // open RocksDB index
    unique_ptr<Index> vindex;
    if (!db_name.empty()) {
        vindex = unique_ptr<Index>(new Index());
        vindex->open_read_only(db_name);
    }

    unique_ptr<xg::XG> xindex;
    if (!xg_name.empty()) {
        xindex = stream::VPKG::load_one<xg::XG>(xg_name);
    }
    
    unique_ptr<GAMIndex> gam_index;
    unique_ptr<stream::ProtobufIterator<Alignment>> gam_cursor;
    if (!sorted_gam_name.empty()) {
        // Load the GAM index
        gam_index = unique_ptr<GAMIndex>(new GAMIndex());
        get_input_file(sorted_gam_name + ".gai", [&](istream& in) {
            // We get it form the appropriate .gai, which must exist
            gam_index->load(in); 
        });
    }

    if (get_alignments) {
        // Dump all the alignments
        if (vindex.get() != nullptr) {
            // Dump from RocksDB
        
            vector<Alignment> output_buf;
            auto lambda = [&output_buf](const Alignment& aln) {
                output_buf.push_back(aln);
                stream::write_buffered(cout, output_buf, 100);
            };
            vindex->for_each_alignment(lambda);
            stream::write_buffered(cout, output_buf, 0);
        } else if (gam_index.get() != nullptr) {
            // Dump from sorted GAM
            // TODO: This is basically a noop.
            get_input_file(sorted_gam_name, [&](istream& in) {
                // Stream the alignments in and then stream them back out.
                stream::for_each<Alignment>(in, stream::emit_to<Alignment>(cout));
            });
        } else {
            cerr << "error [vg find]: Cannot find alignments without a RocksDB index or a sorted GAM" << endl;
            exit(1);
        }
    }

    if (!aln_on_id_range.empty()) {
        // Parse the range
        vector<string> parts = split_delims(aln_on_id_range, ":");
        if (parts.size() == 1) {
            convert(parts.front(), start_id);
            end_id = start_id;
        } else {
            convert(parts.front(), start_id);
            convert(parts.back(), end_id);
        }
        
        if (vindex.get() != nullptr) {
            // Find in RocksDB
            
            // We need a set of all the IDs.
            vector<vg::id_t> ids;
            for (auto i = start_id; i <= end_id; ++i) {
                ids.push_back(i);
            }
            
            vector<Alignment> output_buf;
            auto lambda = [&output_buf](const Alignment& aln) {
                output_buf.push_back(aln);
                stream::write_buffered(cout, output_buf, 100);
            };
            vindex->for_alignment_to_nodes(ids, lambda);
            stream::write_buffered(cout, output_buf, 0);
        } else if (gam_index.get() != nullptr) {
            // Find in sorted GAM
            
            get_input_file(sorted_gam_name, [&](istream& in) {
                // Make a cursor for input
                // TODO: Refactor so we can put everything with the GAM index inside one get_input_file call to deduplicate code
                stream::ProtobufIterator<Alignment> cursor(in);
                
                // Find the alignments and dump them to cout
                gam_index->find(cursor, start_id, end_id, stream::emit_to<Alignment>(cout));
            });
            
        } else {
            cerr << "error [vg find]: Cannot find alignments on range without a RocksDB index or a sorted GAM" << endl;
            exit(1);
        }
    }

    if (!to_graph_file.empty()) {
        // Find alignments touching a graph
        
        // Load up the graph
        ifstream tgi(to_graph_file);
        unique_ptr<VG> graph = unique_ptr<VG>(new VG(tgi));
        
        if (vindex.get() != nullptr) {
            // Collet the IDs in a vector
            vector<vg::id_t> ids;
            graph->for_each_node([&](Node* n) { ids.push_back(n->id()); });
            
            // Throw out the graph
            graph.reset();
        
            // Find in RocksDB
            vector<Alignment> output_buf;
            auto lambda = [&output_buf](const Alignment& aln) {
                output_buf.push_back(aln);
                stream::write_buffered(cout, output_buf, 100);
            };
            vindex->for_alignment_to_nodes(ids, lambda);
            stream::write_buffered(cout, output_buf, 0);
            
        }  else if (gam_index.get() != nullptr) {
            // Find in sorted GAM
            
            // Get the ID ranges from the graph
            auto ranges = vg::algorithms::sorted_id_ranges(graph.get());
            // Throw out the graph
            graph.reset();
            
            get_input_file(sorted_gam_name, [&](istream& in) {
                // Make a cursor for input
                // TODO: Refactor so we can put everything with the GAM index inside one get_input_file call to deduplicate code
                stream::ProtobufIterator<Alignment> cursor(in);
            
                // Find the alignments and send them to cout
                gam_index->find(cursor, ranges, stream::emit_to<Alignment>(cout)); 
            });
            
        } else {
            cerr << "error [vg find]: Cannot find alignments on graph without a RocksDB index or a sorted GAM" << endl;
            exit(1);
        }
        
        
    }

    if (!xg_name.empty()) {
        if (!node_ids.empty() && path_name.empty() && !pairwise_distance) {
            // get the context of the node
            vector<Graph> graphs;
            set<vg::id_t> ids;
            for (auto node_id : node_ids) ids.insert(node_id);
            for (auto node_id : node_ids) {
                Graph g;
                xindex->neighborhood(node_id, context_size, g, !use_length);
                if (context_size == 0) {
                    for (auto& edge : xindex->edges_of(node_id)) {
                        // if both ends of the edge are in our targets, keep them
                        if (ids.count(edge.to()) && ids.count(edge.from())) {
                            *g.add_edge() = edge;
                        }
                    }
                }
                graphs.push_back(g);
            }
            VG result_graph;
            for (auto& graph : graphs) {
                // Allow duplicate nodes and edges (from e.g. multiple -n options); silently collapse them.
                result_graph.extend(graph);
            }
            result_graph.remove_orphan_edges();
            
            // Order the mappings by rank. TODO: how do we handle breaks between
            // different sections of a path with a single name?
            result_graph.paths.sort_by_mapping_rank();
            
            // return it
            result_graph.serialize_to_ostream(cout);
            // TODO: We're serializing graphs all with their own redundant EOF markers if we use multiple functions simultaneously.
        } else if (end_id != 0) {
            for (auto& e : xindex->edges_on_end(end_id)) {
                cout << (e.from_start() ? -1 : 1) * e.from() << "\t" <<  (e.to_end() ? -1 : 1) * e.to() << endl;
            }
        } else if (start_id != 0) {
            for (auto& e : xindex->edges_on_start(start_id)) {
                cout << (e.from_start() ? -1 : 1) * e.from() << "\t" <<  (e.to_end() ? -1 : 1) * e.to() << endl;
            }
        }
        if (!node_ids.empty() && !path_name.empty() && !pairwise_distance && (position_in || rank_in)) {
            // Go get the positions of these nodes in this path
            if (xindex->path_rank(path_name) == 0) {
                // This path doesn't exist, and we'll get a segfault or worse if
                // we go look for positions in it.
                cerr << "[vg find] error, path \"" << path_name << "\" not found in index" << endl;
                exit(1);
            }
            
            // Note: this isn't at all consistent with -P option with rocksdb, which couts a range
            // and then mapping, but need this info right now for scripts/chunked_call
            for (auto node_id : node_ids) {
                cout << node_id;
                for (auto r : (position_in ? xindex->position_in_path(node_id, path_name)
                               : xindex->node_ranks_in_path(node_id, path_name))) {
                    cout << "\t" << r;
                }
                cout << endl;
            }
        }
        if (pairwise_distance) {
            if (node_ids.size() != 2) {
                cerr << "[vg find] error, exactly 2 nodes (-n) required with -D" << endl;
                exit(1);
            }
            cout << xindex->min_approx_path_distance(node_ids[0], node_ids[1]) << endl;
            return 0;
        }
        if (approx_id != 0) {
            cout << xindex->node_start(approx_id) << endl;
            return 0;
        }
        if (list_path_names) {
            size_t m = xindex->max_path_rank();
            for (size_t i = 1; i <= m; ++i) {
                cout << xindex->path_name(i) << endl;
            }
        }
        if (!targets.empty()) {
            Graph graph;
            for (auto& target : targets) {
                // Grab each target region
                string name;
                int64_t start, end;
                parse_region(target, name, start, end);
                if(xindex->path_rank(name) == 0) {
                    // Passing a nonexistent path to get_path_range produces Undefined Behavior
                    cerr << "[vg find] error, path " << name << " not found in index" << endl;
                    exit(1);
                }
                // no coordinates given, we do whole thing (0,-1)
                if (start < 0 && end < 0) {
                    start = 0;
                }
                xindex->get_path_range(name, start, end, graph);
            }
            if (context_size > 0) {
                xindex->expand_context(graph, context_size, true, !use_length);
            }
            VG vgg; vgg.extend(graph); // removes dupes
            
            // Order the mappings by rank. TODO: how do we handle breaks between
            // different sections of a path with a single name?
            vgg.paths.sort_by_mapping_rank();
            
            vgg.serialize_to_ostream(cout);
        }
        if (!range.empty()) {
            Graph graph;
            int64_t id_start=0, id_end=0;
            vector<string> parts = split_delims(range, ":");
            if (parts.size() == 1) {
                cerr << "[vg find] error, format of range must be \"N:M\" where start id is N and end id is M, got " << range << endl;
                exit(1);
            }
            convert(parts.front(), id_start);
            convert(parts.back(), id_end);
            if (!use_length) {
                xindex->get_id_range(id_start, id_end, graph);
            } else {
                // treat id_end as length instead.
                xindex->get_id_range_by_length(id_start, id_end, graph, true);
            }
            if (context_size > 0) {
                xindex->expand_context(graph, context_size, true, !use_length);
            }
            VG vgg; vgg.extend(graph); // removes dupes
            vgg.remove_orphan_edges();
            vgg.serialize_to_ostream(cout);
        }
        if(!haplotype_alignments.empty()) {
            // What should we do with each alignment?
            function<void(Alignment&)> lambda = [&xindex](Alignment& aln) {
                // Count the amtches to the path. The path might be empty, in
                // which case it will yield the biggest size_t you can have.
                size_t matches = xindex->count_matches(aln.path());

                // We do this single-threaded, at least for now, so we don't
                // need to worry about coordinating output, and we can just
                // spit out the counts as bare numbers.
                cout << matches << endl;
            };
            if (haplotype_alignments == "-") {
                stream::for_each(std::cin, lambda);
            } else {
                ifstream in;
                in.open(haplotype_alignments.c_str());
                if(!in.is_open()) {
                    cerr << "[vg find] error: could not open alignments file " << haplotype_alignments << endl;
                    exit(1);
                }
                stream::for_each(in, lambda);
            }

        }
        if (extract_threads) {
            bool extract_reverse = false;
            map<string, list<xg::XG::thread_t> > threads;
            if (extract_thread_patterns.empty()) {
                threads = xindex->extract_threads(extract_reverse);
            } else {
                for (auto& pattern : extract_thread_patterns) {
                    for (auto& t : xindex->extract_threads_matching(pattern, extract_reverse)) {
                        threads[t.first] = t.second;
                    }
                }
            }
            for(auto t : threads) {
                // Convert to a Path
                auto& thread = *t.second.begin();
                auto& thread_name = t.first;
                Path path;
                for(xg::XG::ThreadMapping& m : thread) {
                    // Convert all the mappings
                    Mapping mapping;
                    mapping.mutable_position()->set_node_id(m.node_id);
                    mapping.mutable_position()->set_is_reverse(m.is_reverse);
                    Edit* e = mapping.add_edit();
                    size_t l = xindex->node_length(m.node_id);
                    e->set_from_length(l);
                    e->set_to_length(l);
                    *(path.add_mapping()) = mapping;
                }

                // Get each thread's name
                path.set_name(thread_name);

                // We need a Graph for serialization purposes. We do one chunk per
                // thread in case the threads are long.
                Graph g;
                *(g.add_path()) = path;

                // Dump the graph with its mappings. TODO: can we restrict these to
                vector<Graph> gb = { g };
                stream::write_buffered(cout, gb, 0);
            }
        }
        if (extract_paths) {
            vector<Path> paths;
            for (auto& pattern : extract_path_patterns) {
                for (auto& p : xindex->paths_by_prefix(pattern)) {
                    paths.push_back(p);
                }
            }
            for(auto& path : paths) {
                // We need a Graph for serialization purposes.
                Graph g;
                *(g.add_path()) = xindex->path(path.name());
                // Dump the graph with its mappings. TODO: can we restrict these to
                vector<Graph> gb = { g };
                stream::write_buffered(cout, gb, 0);
            }
        }
        if (!gam_file.empty()) {
            set<vg::id_t> nodes;
            function<void(Alignment&)> lambda = [&nodes](Alignment& aln) {
                // accumulate nodes matched by the path
                auto& path = aln.path();
                if (path.mapping_size() == 1 && !path.mapping(0).has_position() &&
                    path.mapping(0).edit_size() == 1 && edit_is_insertion(path.mapping(0).edit(0))) {
                    // This read is a (presumably full length) insert to no
                    // position. The aligner (used to?) generate these for some
                    // unmapped reads. We should skip it.
                    return;
                }
                for (int i = 0; i < path.mapping_size(); ++i) {
                    nodes.insert(path.mapping(i).position().node_id());
                }
            };
            if (gam_file == "-") {
                stream::for_each(std::cin, lambda);
            } else {
                ifstream in;
                in.open(gam_file.c_str());
                if(!in.is_open()) {
                    cerr << "[vg find] error: could not open alignments file " << gam_file << endl;
                    exit(1);
                }
                stream::for_each(in, lambda);
            }
            // now we have the nodes to get
            Graph graph;
            for (auto& node : nodes) {
                *graph.add_node() = xindex->node(node);
            }
            xindex->expand_context(graph, max(1, context_size), true); // get connected edges
            VG vgg; vgg.extend(graph);
            vgg.serialize_to_ostream(cout);
        }
    } else if (!db_name.empty()) {
        if (!node_ids.empty() && path_name.empty()) {
            // get the context of the node
            vector<VG> graphs;
            for (auto node_id : node_ids) {
                VG g;
                vindex->get_context(node_id, g);
                if (context_size > 0) {
                    vindex->expand_context(g, context_size);
                }
                graphs.push_back(g);
            }
            VG result_graph;
            for (auto& graph : graphs) {
                // Allow duplicate nodes and edges (from e.g. multiple -n options); silently collapse them.
                result_graph.extend(graph);
            }
            result_graph.remove_orphan_edges();
            // return it
            result_graph.serialize_to_ostream(cout);
        } else if (end_id != 0) {
            vector<Edge> edges;
            vindex->get_edges_on_end(end_id, edges);
            for (vector<Edge>::iterator e = edges.begin(); e != edges.end(); ++e) {
                cout << (e->from_start() ? -1 : 1) * e->from() << "\t" <<  (e->to_end() ? -1 : 1) * e->to() << endl;
            }
        } else if (start_id != 0) {
            vector<Edge> edges;
            vindex->get_edges_on_start(start_id, edges);
            for (vector<Edge>::iterator e = edges.begin(); e != edges.end(); ++e) {
                cout << (e->from_start() ? -1 : 1) * e->from() << "\t" <<  (e->to_end() ? -1 : 1) * e->to() << endl;
            }
        }
        if (!node_ids.empty() && !path_name.empty()) {
            int64_t path_id = vindex->get_path_id(path_name);
            for (auto node_id : node_ids) {
                list<pair<int64_t, bool>> path_prev, path_next;
                int64_t prev_pos=0, next_pos=0;
                bool prev_backward, next_backward;
                if (vindex->get_node_path_relative_position(node_id, false, path_id,
                            path_prev, prev_pos, prev_backward,
                            path_next, next_pos, next_backward)) {

                    // Negate IDs for backward nodes
                    cout << node_id << "\t" << path_prev.front().first * (path_prev.front().second ? -1 : 1) << "\t" << prev_pos
                        << "\t" << path_next.back().first * (path_next.back().second ? -1 : 1) << "\t" << next_pos << "\t";

                    Mapping m = vindex->path_relative_mapping(node_id, false, path_id,
                            path_prev, prev_pos, prev_backward,
                            path_next, next_pos, next_backward);
                    cout << pb2json(m) << endl;
                }
            }
        }
        if (!targets.empty()) {
            VG graph;
            for (auto& target : targets) {
                string name;
                int64_t start, end;
                parse_region(target, name, start, end);
                // end coordinate is exclusive for get_path()
                if (end >= 0) {
                    ++end;
                }
                vindex->get_path(graph, name, start, end);
            }
            if (context_size > 0) {
                vindex->expand_context(graph, context_size);
            }
            graph.remove_orphan_edges();
            graph.serialize_to_ostream(cout);
        }
        if (!range.empty()) {
            VG graph;
            int64_t id_start=0, id_end=0;
            vector<string> parts = split_delims(range, ":");
            if (parts.size() == 1) {
                cerr << "[vg find] error, format of range must be \"N:M\" where start id is N and end id is M, got " << range << endl;
                exit(1);
            }
            convert(parts.front(), id_start);
            convert(parts.back(), id_end);
            vindex->get_range(id_start, id_end, graph);
            if (context_size > 0) {
                vindex->expand_context(graph, context_size);
            }
            graph.remove_orphan_edges();
            graph.serialize_to_ostream(cout);
        }
    }

    // todo cleanup if/else logic to allow only one function

    if (!sequence.empty()) {
        if (gcsa_in.empty()) {
            if (get_mems) {
                cerr << "error:[vg find] a GCSA index must be passed to get MEMs" << endl;
                return 1;
            }
            set<int> kmer_sizes = vindex->stored_kmer_sizes();
            if (kmer_sizes.empty()) {
                cerr << "error:[vg find] index does not include kmers, add with vg index -k" << endl;
                return 1;
            }
            if (kmer_size == 0) {
                kmer_size = *kmer_sizes.begin();
            }
            for (int i = 0; i <= sequence.size()-kmer_size; i+=kmer_stride) {
                kmers.push_back(sequence.substr(i,kmer_size));
            }
        } else {
            // let's use the GCSA index

            // Configure GCSA2 verbosity so it doesn't spit out loads of extra info
            gcsa::Verbosity::set(gcsa::Verbosity::SILENT);
            
            // Configure its temp directory to the system temp directory
            gcsa::TempFile::setDirectory(temp_file::get_dir());

            // Open it
            auto gcsa_index = stream::VPKG::load_one<gcsa::GCSA>(gcsa_in);
            // default LCP is the gcsa base name +.lcp
            auto lcp_index = stream::VPKG::load_one<gcsa::LCPArray>(gcsa_in + ".lcp");
            
            //range_type find(const char* pattern, size_type length) const;
            //void locate(size_type path, std::vector<node_type>& results, bool append = false, bool sort = true) const;
            //locate(i, results);
            if (!get_mems) {
                auto paths = gcsa_index->find(sequence.c_str(), sequence.length());
                //cerr << paths.first << " - " << paths.second << endl;
                for (gcsa::size_type i = paths.first; i <= paths.second; ++i) {
                    std::vector<gcsa::node_type> ids;
                    gcsa_index->locate(i, ids);
                    for (auto id : ids) {
                        cout << gcsa::Node::decode(id) << endl;
                    }
                }
            } else {
                // for mems we need to load up the gcsa and lcp structures into the mapper
                Mapper mapper(xindex.get(), gcsa_index.get(), lcp_index.get());
                mapper.fast_reseed = use_fast_reseed;
                // get the mems
                double lcp_avg, fraction_filtered;
                auto mems = mapper.find_mems_deep(sequence.begin(), sequence.end(), lcp_avg, fraction_filtered, max_mem_length, min_mem_length, mem_reseed_length);

                // dump them to stdout
                cout << mems_to_json(mems) << endl;

            }
        }
    }

    if (!kmers.empty()) {
        if (count_kmers) {
            for (auto& kmer : kmers) {
                cout << kmer << "\t" << vindex->approx_size_of_kmer_matches(kmer) << endl;
            }
        } else if (kmer_table) {
            for (auto& kmer : kmers) {
                map<string, vector<pair<int64_t, int32_t> > > positions;
                vindex->get_kmer_positions(kmer, positions);
                for (auto& k : positions) {
                    for (auto& p : k.second) {
                        cout << k.first << "\t" << p.first << "\t" << p.second << endl;
                    }
                }
            }
        } else {
            vector<VG> graphs;
            for (auto& kmer : kmers) {
                VG g;
                vindex->get_kmer_subgraph(kmer, g);
                if (context_size > 0) {
                    vindex->expand_context(g, context_size);
                }
                graphs.push_back(g);
            }

            VG result_graph;
            for (auto& graph : graphs) {
                // Allow duplicate nodes and edges (from multiple kmers); silently collapse them.
                result_graph.extend(graph);
            }
            result_graph.remove_orphan_edges();
            result_graph.serialize_to_ostream(cout);
        }
    }
    
    return 0;

}

static Subcommand vg_msga("find", "use an index to find nodes, edges, kmers, paths, or positions", TOOLKIT, main_find);
