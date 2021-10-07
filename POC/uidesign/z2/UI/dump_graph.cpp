#include <z2/UI/UiDopGraph.h>
#include <z2/UI/UiDopNode.h>
#include <z2/UI/UiDopScene.h>
#include <z2/dop/dop.h>
#include <cctype>


namespace z2::UI {


static std::any parse_any(std::string const &expr) {
    if (!expr.size()) {
        return {};
    }
    if (std::isdigit(expr[0])) {
        if (expr.find('.') != std::string::npos) {  // 3.14
            return std::stof(expr);
        } else {  // 42
            return std::stoi(expr);
        }
    }
    return expr;
}


std::unique_ptr<dop::Graph> UiDopGraph::dump_graph() {
    auto g = std::make_unique<dop::Graph>();

    ztd::map<std::string, dop::Input_Link> exprlut;
    for (auto const &[_, node]: nodes) {
        auto n = g->add_node(node->name, dop::desc_of(node->kind));
        for (int i = 0; i < node->outputs.size(); i++) {
            auto key = node->name + ':' + node->outputs[i]->name;
            exprlut.emplace(key, dop::Input_Link{.node = n, .sockid = i});
        }
        n->xpos = node->position.x;
        n->name = node->name;
        n->desc = &dop::desc_of(node->kind);
    }

    for (auto const &[_, node]: nodes) {
        auto n = g->get_node(node->name);

        for (int i = 0; i < node->inputs.size(); i++) {
            dop::Input input;

            auto *socket = node->inputs[i];
            if (socket->links.size()) {
                auto *link = *socket->links.begin();
                auto *another = link->from_socket;

            } else {
                auto expr = socket->value;

                if (expr.starts_with('@')) {
                    expr = expr.substr(1);
                    auto p = expr.find(':');
                    if (p == std::string::npos) {  // @Route1
                        auto outnode = g->get_node(expr);
                        input = dop::Input_Link{.node = outnode, .sockid = 0};
                    } else {
                        auto sockname = expr.substr(p + 1);
                        auto nodename = expr.substr(0, p);
                        auto *outnode = g->get_node(nodename);
                        if (sockname.size() && std::isdigit(sockname[0])) {  // @Route1:0
                            int outid = std::stoi(sockname);
                            input = dop::Input_Link{.node = outnode, .sockid = outid};
                        } else if (sockname.size()) {  // @Route1:value
                            input = exprlut.at(expr);
                        }
                    }
                } else {  // 3.14
                    input = dop::Input_Value{.value = parse_any(expr)};
                }
            }

            n->inputs.at(i) = input;
        }
    }
    return g;
}


}