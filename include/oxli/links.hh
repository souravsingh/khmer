/*
This file is part of khmer, https://github.com/dib-lab/khmer/, and is
Copyright (C) 2015-2016, The Regents of the University of California.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * Neither the name of the Michigan State University nor the names
      of its contributors may be used to endorse or promote products
      derived from this software without specific prior written
      permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
LICENSE (END)

Contact: khmer-project@idyll.org
*/
#ifndef LINKS_HH
#define LINKS_HH

#include <algorithm>
#include <functional>
#include <memory>
#include <list>
#include <iostream>

#include "oxli.hh"
#include "kmer_hash.hh"
#include "hashtable.hh"
#include "hashgraph.hh"
#include "kmer_filters.hh"
#include "traversal.hh"
#include "assembler.hh"


namespace oxli {

struct Junction {
    // [u]-->[v (HDN)]-->[w]
    HashIntoType u;
    HashIntoType v;
    HashIntoType w;
    uint64_t count;
    Junction() = default;
    HashIntoType id() const { return u ^ v ^ w; }
    bool matches(HashIntoType u, HashIntoType v) { return (u ^ v ^ w) == id(); }

    friend std::ostream& operator<< (std::ostream& stream,
                                    const Junction& j);
    friend bool operator== (const Junction& lhs,
                            const Junction& rhs) {
        return lhs.id() == rhs.id();
    }
};


typedef std::list<Junction*> JunctionList;

#define FW 1
#define RC 0

/*
class LinkNode {
private:
    static uint64_t counter;

public:

    LinkNode* next;
    Junction* junction;
    HashIntoType from;
    uint64_t node_id;

    LinkNode(Junction* junction, HashIntoType from) :
        junction(junction), from(from), time_created(time_created),
        node_id(counter), next(nullptr)
    {
        ++counter;
    }
};


class LinkHead {
private:
    static uint64_t counter;
public:
    uint64_t link_id;
    LinkNode* start;
    HashIntoType from;
    bool forward;

    LinkHead(LinkNode* start, HashIntoType from, bool forward,
             uint64_t time_created) : 
        start(start), from(from), forward(forward), time_created(time_created)
    {
        ++counter;
    }
};
*/             

class Link {
private:

    static uint64_t n_links;

protected:

    JunctionList junctions;

public:

    const bool forward;
    const uint64_t link_id;
    const uint64_t time_created; // in "read space" ie read number
    //size_t extent;
    uint64_t flanking_distance;
    
    Link(uint64_t time_created,
         bool forward=true) :
        forward(forward), link_id(n_links),
        time_created(time_created),
        flanking_distance(0)
    {
        n_links++;
    }
    inline void push_back(Junction* junction)
    {
        //if (junctions.size() == 0) {
        //    junction->start = true;
            //junction->distance_prev = 0;
        //}
        junctions.push_back(junction);
    }

    inline void push_front(Junction* junction)
    {
        //if (junctions.size() > 0) {
        //    start_junction()->start = false;
        //}
        //junction->start = true; // just to be sure
        //junction->distance_prev = 0;
        junctions.push_front(junction);
    }

    void insert_junction(JunctionList::iterator insert_before,
                         Junction* junction)
    {
        if (insert_before == begin()) {
            push_front(junction);
        } else {
            junctions.insert(insert_before, junction);
        }
    }

    inline Junction* start_junction() const
    {
        return junctions.front();
    }

    inline Junction* end_junction() const
    {
        return junctions.back();
    }

    inline bool is_forward() const
    {   
        return forward;
    }

    inline size_t size() const
    {
        return junctions.size();
    }

    inline JunctionList::iterator begin() 
    {
        return junctions.begin();
    }

    inline JunctionList::iterator end() 
    {
        return junctions.end();
    }

    JunctionList& get_junctions() {
        return junctions;
    }

};


class LinkCursor
{
public:
    Link* link;
    uint64_t traversal_age;
    JunctionList::iterator cursor;

    LinkCursor(Link* link, uint64_t age) :
        link(link), traversal_age(age), cursor(link->begin())
    {
    }

    bool done() {
        return cursor == link->end();
    }

    const Junction* current() const {
        const Junction* current = *cursor;
        return current;
    }

    bool increment() {
        ++cursor;
        return done();
    }

    friend bool operator==(const LinkCursor& lhs, const LinkCursor& rhs)
    {
        return lhs.link == rhs.link;
    }
};


// Compare the two link cursors first by their age within the
// traversal, then by their global age. We prefer links
// that were found earlier in the traversal, but were created
// most recently.
inline bool CompareLinks(const LinkCursor& a, const LinkCursor& b)
{
    if (a.traversal_age < b.traversal_age) { return false; }
    if (b.traversal_age < a.traversal_age) { return true; }

    if ((a.link)->time_created > (b.link)->time_created) { return false; }
    if ((b.link)->time_created > (a.link)->time_created) { return true; }

    return false;
}

/*
class LinkTraversal
{
    typedef std::priority_queue<LinkCursor, 
                                        std::vector<LinkCursor>,
                                        CompareLinks> CursorQueue;
    std::shared_ptr<CursorQueue> link_cursors;
    std::shared_ptr<std::set<HashIntoType>> constraints; // constraint junction ids
    LinkCursor last_link;

    LinkTraversal() : has_active_cursor(false)
    {
        link_cursors = std::make_shared<CursorQueue>();
    }

    void add_links(std::shared_ptr<LinkList> links,
                   uint64_t age)
    {
        for (Link* link: &links) {
            link_cursors->push(LinkCursor(link, age));
        }
    }

    void pop_all(Junction* to_pop)
    {
        for (LinkCursor cursor : &link_cursors) {
            if (!cursor.done() && (to_pop == cursor.current())) {
                cursor.increment();
            }
        }
    }

    bool get_top_link(&LinkCursor result)
    {
        while(link_cusors->size() > 0) {
            if (!link_cusors->top()->done()) {
                result = link_cusors->top();
                return true;
            } else {
                link_cusors->pop();
            }
        }
        return false;
    }

    template<bool direction>
    bool try_link_neighbors(AssemblerTraverser<direction> asmt)
    {
        KmerQueue neighbors;
        asmt.neighbors(neighbors);
        bool decided = false;
        Kmer src = asmt.cursor;
        Kmer dst;

        LinkCursor link;
        bool has_link = get_top_link(link);
        if (!has_link) {
            return false;
        }

        if (has_active_cursor) {

            for (Kmer neighbor : neighbors) {
                if ((&active_cursor.current())->matches(neighbor.kmer_f,
                                                        neighbor.kmer_r)) {
                    decided = true;
                    dst     = neighbor;
                }
            }


        } else {


        }
        
    }



};
*/


typedef std::unordered_multimap<HashIntoType, Link*> LinkMap;
typedef std::list<Link*> LinkList;
typedef std::unordered_map<HashIntoType, Junction*> JunctionMap;
typedef std::pair<HashIntoType, Link*> LinkMapPair;

class GraphLinker
{
protected:
    // map from starting high degree nodes to associated links
    LinkMap links;
    // map from junction keys to Junction*
    JunctionMap junctions;
    uint64_t n_sequences_added;

public:

    std::shared_ptr<Hashgraph> graph;
    
    GraphLinker(std::shared_ptr<Hashgraph> graph) :
        graph(graph), n_sequences_added(0)
    {

    }

    WordLength ksize() const
    {
        return graph->ksize();
    }

    void report() const
    {
        std::cout << "GraphLinker (@" << this << " with "
            << "Hashgraph @" << graph.get() << ")" << std::endl;
        std::cout << "  * " << junctions.size() << " junctions" << std::endl;
        std::cout << "  * " << n_sequences_added << " sequences added" << std::endl;
        std::cout << "  * " << links.size() << " links" << std::endl;
    }

    Junction* get_junction(HashIntoType key) const
    {
        auto search = junctions.find(key);
        if (search != junctions.end()) {
            return search->second;
        }
        return nullptr;
    }

    Junction* get_junction(HashIntoType u, HashIntoType v, HashIntoType w) const
    {
        return get_junction(u ^ v ^ w);
    }

    Junction* get_junction(Junction& junction) const
    {
        return get_junction(junction.id());
    }

    Junction* new_junction(HashIntoType u, HashIntoType v, HashIntoType w)
    {
        Junction* j = new Junction();
        j->u = u;
        j->v = v;
        j->w = w;
        j->count = 0;
        junctions[j->id()] = j;
        return j;
    }

    Junction* fetch_or_new_junction(HashIntoType u, HashIntoType v, 
                                    HashIntoType w, uint64_t& counter)
    {
        Junction* j = get_junction(u, v, w);
        if (j != nullptr) {
            j->count = j->count + 1;
        } else {
            counter++;
            j = new_junction(u, v, w);
            j->count = 1;
        }
        return j;
    }

    std::shared_ptr<JunctionList> get_junctions(const std::string& sequence) const
    {
        KmerIterator kmers(sequence.c_str(), graph->ksize());
        std::shared_ptr<JunctionList> junctions = make_shared<JunctionList>();

        Kmer u = kmers.next();
        if (kmers.done()) {
            return junctions;
        }
        Kmer v = kmers.next();
        if (kmers.done()) {
            return junctions;
        }
        Kmer w = kmers.next();

        Junction* j;

        while(!kmers.done()) {
            j = get_junction(u, v, w);
            if (j != nullptr) {
                junctions->push_back(j);
            }
            u = v;
            v = w;
            w = kmers.next();
        }

        return junctions;
    }
 
    void build_links(const std::string& sequence,
                     Link* &fw_link, Link* &rc_link)
    {
        std::cout << "build_links()" << std::endl;
        KmerIterator kmers(sequence.c_str(), graph->ksize());
        Kmer u, v, w;
        uint64_t d = 0;

        u = kmers.next();
        ++d;
        if (kmers.done()) {
            fw_link = rc_link = nullptr;
            return;
        }
        v = kmers.next();
        ++d;
        if (kmers.done()) {
            fw_link = rc_link = nullptr;
            return;
        }
        w = kmers.next();
        ++d;
    
        std::cout << "  - build_links: allocate new Link*" << std::endl;
        fw_link = new Link(n_sequences_added, u.is_forward());
        rc_link = new Link(n_sequences_added, !u.is_forward());
        uint64_t n_new_fw = 0;
        uint64_t n_new_rc = 0;
        uint64_t last_rc_pos = 0;

        Traverser traverser(graph.get());
        while(!kmers.done()) {
            if (traverser.degree_right(v) > 1) {
                std::cout << "  - build_links: found FW HDN " << u << std::endl;
                fw_link->push_back(fetch_or_new_junction(u, v, w, n_new_fw));
                if (n_new_fw == 1) {
                    fw_link->flanking_distance = d;
                }
            }
            if (traverser.degree_left(v) > 1) {
                std::cout << "  - build_links: found RC HDN " << v << std::endl;
                rc_link->push_front(fetch_or_new_junction(w, v, u, n_new_rc));
                last_rc_pos = d;
            }

            u = v;
            v = w;
            w = kmers.next();
            ++d;
        }

        if (fw_link->size() < 1) {
            std::cout << "  - build_links: (fw_link) no (new) junctions found." << std::endl;
            delete fw_link;
            fw_link = nullptr;
        }

        if (rc_link->size() < 1) {
            std::cout << "  - build_links: (rc_link) no (new) junctions found." << std::endl;
            delete rc_link;
            rc_link = nullptr;
        } else {
            rc_link->flanking_distance = d - last_rc_pos;
        }
    }


    void add_links(const std::string& sequence)
    {
        std::cout << "add_links('" << sequence << "')" << std::endl;
        Link * fw_link = nullptr;
        Link * rc_link = nullptr;
        n_sequences_added++;
        build_links(sequence, fw_link, rc_link);

        if (fw_link != nullptr) {
            std::cout << "  - add_links: insert found fw_link" << std::endl;
            Junction* start = fw_link->start_junction();
            std::cout << "    * start junction: " << &start << std::endl;
            links.insert(LinkMapPair(start->v, fw_link));
        }
        if (rc_link != nullptr) {
            std::cout << "  - add_links: insert found rc_link" << std::endl;
            Junction* start = rc_link->start_junction();
            std::cout << "    * start junction: " << &start << std::endl;
            links.insert(LinkMapPair(start->v, rc_link));
        }
    }

    uint64_t get_links(Kmer hdn, std::shared_ptr<LinkList> found_links)
    {
        auto range = links.equal_range(hdn);
        uint64_t n_links_found = 0;
        for (auto it = range.first; it != range.second; ++it) {
            found_links->push_back(it->second);
            n_links_found++;
        }
        return n_links_found;
    }

    uint64_t get_links(std::list<Kmer> high_degree_nodes, 
                   std::shared_ptr<LinkList> found_links)
    {
        uint64_t n_links_found = 0;
        for (Kmer hdn : high_degree_nodes) {
            n_links_found += get_links(hdn, found_links);
        }
        return n_links_found;
    }

    std::shared_ptr<LinkList> get_links(const std::string& sequence)
    {
        std::cout << "get_links('" << sequence << "')" << std::endl;
        KmerIterator kmers(sequence.c_str(), graph->ksize());
        Traverser traverser(graph.get());
        std::list<Kmer> hdns;

        std::cout << "  - get_link: start iterating k-mers" << std::endl;
        Kmer kmer = kmers.next();
        while(!kmers.done()) {
            if (traverser.degree(kmer) > 2) {
                hdns.push_back(kmer);
            }
            kmer = kmers.next();
        }
        std::cout << "  - get_links: found " << hdns.size() << " hdns" << std::endl;

        std::shared_ptr<LinkList> found_links = make_shared<LinkList>();
        get_links(hdns, found_links);
        std::cout << "  - get_links: returning " << found_links->size() << " links" << std::endl;
        return found_links;
    }

};

/*

class LinkedAssembler
{
    std::shared_ptr<LinearAssembler> linear_asm;

public:

    const std::shared_ptr<Hashgraph> graph;
    const std::shared_ptr<GraphLinker> linker;
    WordLength _ksize;

    explicit LinkedAssembler(std::shared_ptr<GraphLinker> linker) :
        _ksize(linker->ksize()), graph(linker->graph), linker(linker)
    {
        linear_asm = make_shared<LinearAssembler>(graph.get());
    }

    StringVector assemble(const Kmer seed_kmer,
                          std::shared_ptr<Hashgraph> stop_bf=nullptr) const;

    template <bool direction>
    void _assemble_directed(AssemblerTraverser<direction>& cursor,
                            StringVector& paths) const;
};
*/


}




#endif