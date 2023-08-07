// baichen318@gmail.com


#ifndef VISUALIZER_H
#define VISUALIZER_H


#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/property_map/dynamic_property_map.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/graph/properties.hpp>
#include <string>
#include <sstream>
#include <ostream>
#include <iostream>
#include <cassert>
#include <cstdlib>
#include <vector>
#include <set>
#include <utility>
#include <unordered_map>
#include <algorithm>
#include "graph.h"


static const std::unordered_map<int, std::string> name_of_vertex_type = {
	std::pair<int, std::string>(0, "fetch"),
	std::pair<int, std::string>(1, "dispatch"),
	std::pair<int, std::string>(2, "execute"),
	std::pair<int, std::string>(3, "mem. execute"),
	std::pair<int, std::string>(4, "commit"),
	std::pair<int, std::string>(5, "undefined")
};


struct vertex_property {
	struct graphviz_property {
		explicit graphviz_property(std::pair<int, int> _pos): pos(_pos) {}

		explicit graphviz_property(std::pair<int, int> _pos, std::string _xlabel): pos(_pos), xlabel(_xlabel) {}

		explicit graphviz_property() = default;

		std::pair<int, int> pos;
		std::string xlabel;
	};

	explicit vertex_property(int _type, uint64_t _inst_num, string _inst): type(_type), inst_num(_inst_num), inst(_inst) {}

	explicit vertex_property(
		int _type,
		uint64_t _inst_num,
		string _inst,
		graphviz_property _property
	): type(_type), inst_num(_inst_num), inst(_inst), property(_property) {}

	vertex_property() = default;

	int type;
	uint64_t inst_num;
	string inst;
	graphviz_property property;
};


struct edge_property {
	struct graphviz_property {
		explicit graphviz_property(bool _dashed): dashed(_dashed) {}

		explicit graphviz_property() = default;

		bool dashed;
	};

	explicit edge_property(std::string _weight): weight(_weight) {}

	explicit edge_property(std::string _weight, graphviz_property _property): weight(_weight), property(_property) {}

	edge_property() = default;

	std::string weight;
	graphviz_property property;
};


std::string graphviz_encode(std::string s) noexcept {
	// boost::algorithm::replace_all(s, ",", "$$$COMMA$$$");
	// boost::algorithm::replace_all(s, " ", "$$$SPACE$$$");
	// boost::algorithm::replace_all(s, "\"", "$$$QUOTE$$$");
	return s;
}


boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, vertex_property, edge_property>
create_empty_directed_graph() noexcept {
	return {};
}


template <typename graph>
std::vector<vertex_property> get_vertices(const graph & g) noexcept {
	std::vector<vertex_property> v(boost::num_vertices(g));
	const auto vp = vertices(g);
	std::transform(vp.first, vp.second, std::begin(v), [&g](auto & _v) {
			return g[_v];
		}
	);
	return v;
}


template <typename graph, typename _vertex_property>
typename boost::graph_traits<graph>::vertex_iterator get_vertex(_vertex_property & v, const graph & g) noexcept {
	const auto vp = vertices(g);
	return std::find_if(vp.first, vp.second, [&v, &g](auto _v) {
			return v.type == g[_v].type && v.inst_num == g[_v].inst_num;
		}
	);
}


template <typename graph, typename _vertex_property>
typename boost::graph_traits<graph>::vertex_descriptor add_vertex_with_property(const _vertex_property & v, graph & g) noexcept {
	static_assert(!std::is_const<graph>::value, "[ERROR]: graph cannot be const.");
	auto _v = get_vertex(v, g);
	if (_v == vertices(g).second)
		return boost::add_vertex(v, g);
	else
		return *_v;
}


template <typename graph>
bool if_edge_exists_between_vertices(
	const typename boost::graph_traits<graph>::vertex_descriptor & v1,
	const typename boost::graph_traits<graph>::vertex_descriptor & v2,
	const graph & g
) noexcept {
	return edge(v1, v2, g).second;
}


template <typename graph, typename _edge_property>
typename boost::graph_traits<graph>::edge_descriptor add_edge_with_property(
	const typename boost::graph_traits<graph>::vertex_descriptor & from,
	const typename boost::graph_traits<graph>::vertex_descriptor & to,
	const _edge_property & edge,
	graph & g
) {
	static_assert(!std::is_const<graph>::value, "[ERROR]: graph cannot be const.");
	if (if_edge_exists_between_vertices(from, to, g)) {
		std::stringstream msg;
		msg << "[ERROR]: " << __func__ << ": already has an edge";
		// throw std::invalid_argument(msg.str());
	}
	const auto e = boost::add_edge(from, to, g);
	assert(e.second);
	g[e.first] = edge;
	return e.first;
}


template <typename graph>
class vertex_writer {
public:
	vertex_writer(graph g): _g(g) {}

	template <class vertex_descriptor>
	void operator()(std::ostream & out, const vertex_descriptor & vd) const noexcept {
		out << "[label=\"" << graphviz_encode(
				std::to_string(_g[vd].inst_num) + ' ' + (name_of_vertex_type.at(_g[vd].type))
			) << "\",pos=\"" << _g[vd].property.pos.first << "," << _g[vd].property.pos.second  << "!\"";
		if (_g[vd].inst != "") {
			out << ",xlabel=\"" << _g[vd].inst << "\"]";
		} else {
			out << "]";
		}
	}

private:
	graph _g;
};


template <typename graph>
class edges_writer {
public:
	edges_writer(graph g): _g(g) {}

	template <class edge_descriptor>
	void operator()(std::ostream & out, const edge_descriptor & ed) const noexcept {
		out << "[label=\"" << graphviz_encode(_g[ed].weight) << "\"";
		if (_g[ed].property.dashed) {
			out << ",style=\"dashed\"" << ",splines=\"curved\",color=\"red\"" << "]";
		} else {
			out << "]";
		}
	}

private:
	graph _g;
};


template <typename graph>
inline vertex_writer<graph> make_vertex_writer(const graph & g) {
	return vertex_writer<graph>(g);
}


template <typename graph>
inline edges_writer<graph> make_edges_writer(const graph & g) {
	return edges_writer<graph>(g);
}


class Visualizer : public Graph {
public:

	using deg_t = std::unordered_map<Vertex, vector<OutgoingEdge>, VertexHash, VertexEqual>;
	using bg_t = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, vertex_property, edge_property>;

	Visualizer(
		deg_t graph
	): Graph() {
		_graph = deg2bg(graph);
	}

	bg_t deg2bg(deg_t);

	void generate_dot(const std::string &);

	void run() {};

	~Visualizer();
	
private:
	bg_t _graph;
};


boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, vertex_property, edge_property> Visualizer::deg2bg(deg_t graph) {
	auto bg = create_empty_directed_graph();
	std::for_each(graph.begin(), graph.end(), [&bg](auto node) {
			auto parent = node.first;
			std::for_each(node.second.begin(), node.second.end(), [&parent, &bg](auto deg_edge) {
					auto v1 = add_vertex_with_property(
						vertex_property(
							parent.type,
							parent.instrNum,
							parent.inst,
							vertex_property::graphviz_property(std::pair<int, int>(parent.instrNum, -parent.type))
						),
						bg
					);
					auto v2 = add_vertex_with_property(
						vertex_property(
							deg_edge.child.type,
							deg_edge.child.instrNum,
							deg_edge.child.inst,
							vertex_property::graphviz_property(std::pair<int, int>(deg_edge.child.instrNum, -deg_edge.child.type))
						),
						bg
					);
					auto dashed = false;
					if (parent.type == deg_edge.child.type && (deg_edge.child.instrNum - parent.instrNum) > 1) {
						// vertices are on the same x-axis
						dashed = true;
					}
					auto weight = deg_edge.weight.toString();
					weight.erase(std::remove_if(weight.begin(), weight.end(), ::isspace), weight.end());
					add_edge_with_property(
						v1,
						v2,
						edge_property(
							weight,
							edge_property::graphviz_property(dashed)
						),
						bg
					);
				}
			);
		}
	);
	return bg;
}


void Visualizer::generate_dot(const std::string & file_name) {
	std::ofstream f(file_name);
	boost::write_graphviz(
		f,
		_graph,
		make_vertex_writer(_graph),
		make_edges_writer(_graph)
	);
	std::exit(EXIT_SUCCESS);
}


Visualizer::~Visualizer() {
	
}


#endif // VISUALIZER_H
