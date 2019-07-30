#include "mcmc_genotyper.hpp"
#include "algorithms/count_walks.hpp"
#include "subgraph.hpp"
#include <algorithm> 
#include <random> 
#include <chrono>
#include <utility>

namespace vg {

    using namespace std;

    MCMCGenotyper::MCMCGenotyper(SnarlManager& snarls, VG& graph, const int n_iterations):snarls(snarls), graph(graph), n_iterations(n_iterations), 
        random_engine(chrono::system_clock::now().time_since_epoch().count()){
            // will want to implement a way for callee to pass in the seed value for predictability in unit testing 
    
    }

    unique_ptr<PhasedGenome> MCMCGenotyper::run_genotype(const vector<MultipathAlignment>& reads, const double log_base) const{

        // generate initial value 
        unique_ptr<PhasedGenome> genome = generate_initial_guess();
        
    
        for(int i = 1; i<= n_iterations; i++){
            
            // holds the previous sample allele
            double x_star_log = log_target(*genome, reads);
        
            // get contents from proposal_sample
            tuple<int, const Snarl*, vector<NodeTraversal> > to_receive = proposal_sample(*genome);   
            int& modified_haplo = get<0>(to_receive);
            // if the to_receive contents are invalid, do not swap
            if (modified_haplo ==-1){
                continue;
            }else{
                const Snarl& modified_site = *get<1>(to_receive); 
                vector<NodeTraversal>& old_allele = get<2>(to_receive); 
 
                // holds new sample allele
                double current_log = log_target(*genome, reads);

                // calculate likelihood ratio of posterior distribution 
                double likelihood_ratio = exp(log_base*(current_log - x_star_log));

                // calculate acceptance probability 
                double acceptance_probability = min(1.0, likelihood_ratio);
            
                if(runif(0.0,1.0) > acceptance_probability){ 
                    genome->set_allele(modified_site, old_allele.begin(), old_allele.end(), modified_haplo); 
                }
                
            }

        }
        //genome->print_phased_genome();
        return std::move(genome); 

    }   
    double MCMCGenotyper::log_target(PhasedGenome& phased_genome, const vector<MultipathAlignment>& reads)const{
        
        // sum of scores given the reads aligned on the haplotype 
        double sum_scores; 
        
        // condition on data 
        for(MultipathAlignment mp : reads){
            sum_scores += phased_genome.optimal_score_on_genome(mp, graph);
        } 
        return sum_scores;
    }

    tuple<int, const Snarl*, vector<NodeTraversal> > MCMCGenotyper::proposal_sample(PhasedGenome& current)const{
        // get a different traversal through the snarl by uniformly choosing from all possible ways to traverse the snarl
        
        // bookkeeping: haplotype ID, snarl* (site that we replaced at), get_allele())
        tuple<int, const Snarl*, vector<NodeTraversal> > to_return;

        int& random_haplotype = get<0>(to_return);
        const Snarl*& random_snarl = get<1>(to_return);
        // the random path through the snarl 
        vector<NodeTraversal>& allele = get<2>(to_return);


        // set a seed for predictability in testing 
        unsigned seed = 1; 
        minstd_rand0 random_engine(seed);
        
        // sample uniformly between snarls 
        random_snarl = snarls.discrete_uniform_sample(random_engine);

        if(random_snarl == nullptr){
            random_haplotype = -1;
            return to_return;
        }

        // get list of haplotypes that contain the snarl
        vector<id_t> matched_haplotypes = current.get_haplotypes_with_snarl(random_snarl);


        if(matched_haplotypes.empty()){
            random_haplotype = -1;
            return to_return;
            
        }

        // choose a haplotype uiformly 
        id_t lower_bound = 0;
        id_t upper_bound = matched_haplotypes.size();
        int random_num = generate_discrete_uniform(random_engine, lower_bound, upper_bound);
        
        random_haplotype = matched_haplotypes[random_num];

        pair<unordered_set<id_t>, unordered_set<edge_t> > contents = snarls.deep_contents(random_snarl, graph, false);

        // unpack the pair, we only care about the node_ids
        unordered_set<id_t>& ids = contents.first;
            
        // enumerate counts through nodes in snarl not the entire graph
        SubHandleGraph subgraph(&graph);
        
        for (id_t id : ids){
            
            // for each node_id in the snarl, get handle
            subgraph.add_handle(subgraph.get_handle(id, false));
        }
        
        // create a count_map
        auto count_contents = algorithms::count_walks_through_nodes(&graph);

        // unpack the count map from the count_contents 
        unordered_map<handle_t, size_t>& count_map = get<1>(count_contents);
        handle_t& source_handle = get<3>(count_contents); //get the source 
        
        // build the topology of the count_map
        vector<size_t> cumulative_sum;
        vector<size_t> paths_to_take; // will store the counts 
        bool start_from_sink =true;
        size_t  count = 0, cum_sum = 0, chosen  =0;
        vector<handle_t> handles;
        
        // create a topological order of the map
        vector<handle_t> topological_order = algorithms::lazier_topological_order(&graph);

        //  we want to get just one handle
        handle_t start = topological_order[0]; 

        // start at sink in topological
        bool not_source = true;
        
        while(not_source){

            graph.follow_edges(start, start_from_sink, [&](const handle_t& next) { 
                unordered_map<handle_t, size_t>::iterator it;
                it= count_map.find(next); // find the handle
                count = it->second; // get the count 
                paths_to_take.push_back(count);
                cum_sum += count;
                cumulative_sum.push_back(cum_sum); 
                handles.push_back(next); 
        
            });
            
            // choose a random path uniformly
            id_t l_bound = 1;
            id_t u_bound = cumulative_sum[cumulative_sum.size()-1];   // cumulative sum will be the value at the last element 
            int random_num = generate_discrete_uniform(random_engine,l_bound, u_bound);


            int found = -1;
            for (int i = 0; i<= cumulative_sum.size() ; i++) {
                if (cumulative_sum[i] <= random_num && random_num < cumulative_sum[i] ){
                    chosen = paths_to_take[i]; // chosen will contain the count of the handle we want to take
                    found = i;
                    break; 
                }else{
                    continue;
                }
            } 
            if(found != -1){
                handle_t random_handle = handles[found]; // this is the handle we want to start from
                bool position = graph.get_is_reverse(random_handle);
                Node* n = graph.get_node(graph.get_id(random_handle));
                allele.push_back(NodeTraversal(n,false));
                // follow random _handle
                start = random_handle; 
            }   
            
            

            // check if the random handle is the source, if so we terminate loop
            if(start == source_handle){
                not_source = false;
            }    

        }
        // get allele at site where we will make replacement 
        vector<NodeTraversal> old_allele = get<2>(to_return);
        old_allele = current.get_allele(*random_snarl, random_haplotype);
        

        // set new allele with random allele, replace with previous allele 
        current.set_allele(*random_snarl , allele.begin(), allele.end(), random_haplotype);
        
 
        return to_return;

    }
    int MCMCGenotyper::generate_discrete_uniform(minstd_rand0& random_engine, id_t lower_bound , id_t upper_bound) const{
        
        // choose a number randomly using discrete uniform distribution
        uniform_int_distribution<int> distribution(lower_bound, upper_bound);  
        int random_num = distribution(random_engine);

        return random_num;
    }
    double MCMCGenotyper::runif(const double a, const double b)const{
        
        uniform_real_distribution<double> distribution(a,b);
        double random_num = distribution(random_engine);
        
        return random_num;

    }
    unique_ptr<PhasedGenome> MCMCGenotyper::generate_initial_guess()const{
        
        unique_ptr<PhasedGenome> genome(new PhasedGenome(snarls));
        vector<NodeTraversal> haplotype; //will add twice  

        graph.for_each_path_handle([&](const path_handle_t& path){
        // capture all variables (paths) in scope by reference 

            if(!Paths::is_alt(graph.get_path_name(path))) {
            // If it isn't an alt path, we want to trace it
  
                for (handle_t handle : graph.scan_path(path)) {
                // For each occurrence from start to end
                    
                    // get the node and the node postion and add to the vector 
                    Node* node = graph.get_node(graph.get_id(handle));
                    bool position = graph.get_is_reverse(handle);
                    haplotype.push_back(NodeTraversal(node,position));
  
                }
            }
        });
        // construct haplotypes
        // haplotype1 = haplotype2
        genome->add_haplotype(haplotype.begin(), haplotype.end());
        genome->add_haplotype(haplotype.begin(), haplotype.end());

        return std::move(genome);
    }

}


