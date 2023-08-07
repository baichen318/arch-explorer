# baichen.bai@alibaba-inc.com


from graphviz import Digraph
from utils.utils import error
from utils.exceptions import UnSupportedException


class Visualization(object):

    THEMES = {
        "basic": {
            "background_color": "#FFFFFF",
            "fill_color": "#E8E8E8",
            "outline_color": "#000000",
            "font_color": "#000000",
            "font_name": "Times",
            "font_size": "10",
            "margin": "0,0",
            "padding":  "1.0,0.5",
        },
        "blue": {
            "background_color": "#FFFFFF",
            "fill_color": "#BCD6FC",
            "outline_color": "#7C96BC",
            "font_color": "#202020",
            "font_name": "Verdana",
            "font_size": "10",
            "margin": "0,0",
            "padding":  "1.0,0.5",
        },
    }

    def __init__(self, model, theme="basic"):
        super(Visualization, self).__init__()
        self.model = model
        self.theme = self.initialize_theme(theme)

    def initialize_theme(self, theme):
        return Visualization.THEMES[theme]

    def draw_deg(self, dot, graph, start, end):
        if start == -1 or end == -1:
            # we draw all nodes and edges
            for pipeline_stage in graph.nodes:
                node = graph.nodes[pipeline_stage]["node"]
                dot.node(
                    pipeline_stage,
                    node.__repr__(),
                    pos="{},{}!".format(
                        node.coordinate[0],
                        node.coordinate[1]
                    )
                )
            for u, v in graph.edges:
                dot.edge(
                    u, v,
                    label=self.model.get_edge(u, v).__repr__()
                )
        else:
            # we draw nodes and edges between `start` & `end`.
            for pipeline_stage in graph.nodes:
                seq = self.model.get_seq_via_name(pipeline_stage)
                if seq >= start and seq <= end:
                    node = graph.nodes[pipeline_stage]["node"]
                    dot.node(
                        pipeline_stage,
                        node.__repr__(),
                        pos="{},{}!".format(
                            node.coordinate[0],
                            node.coordinate[1]
                        )
                    )
            for u, v in graph.edges:
                u_seq = self.model.get_seq_via_name(u)
                v_seq = self.model.get_seq_via_name(v)
                edge = self.model.get_edge(u, v)
                if u_seq >= start and u_seq <= end and \
                    v_seq >= start and v_seq <= end:
                    if edge.critical:
                        color = "red"
                    elif edge.virtual:
                        color = "blue"
                    else:
                        color = "black"
                    dot.edge(
                        u, v,
                        label=self.model.get_edge(u, v).__repr__(),
                        color=color
                    )
        return dot

    def draw(self, graph, start: int, end: int):
        dot = Digraph(engine="neato")
        dot.attr("graph", 
             bgcolor=self.theme["background_color"],
             color=self.theme["outline_color"],
             fontsize=self.theme["font_size"],
             fontcolor=self.theme["font_color"],
             fontname=self.theme["font_name"],
             margin=self.theme["margin"],
             rankdir="LR",
             pad=self.theme["padding"]
        )
        dot.attr("node", shape="ellipse", 
             style="filled", margin="0,0",
             fillcolor=self.theme["fill_color"],
             color=self.theme["outline_color"],
             fontsize=self.theme["font_size"],
             fontcolor=self.theme["font_color"],
             fontname=self.theme["font_name"]
        )
        dot.attr("edge", style="solid", 
             color=self.theme["outline_color"],
             fontsize=self.theme["font_size"],
             fontcolor=self.theme["font_color"],
             fontname=self.theme["font_name"]
        )
        return self.draw_deg(dot, graph, start, end)
