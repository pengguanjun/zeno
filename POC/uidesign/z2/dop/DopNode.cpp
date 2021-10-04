#include <z2/dop/DopNode.h>
#include <z2/dop/DopFunctor.h>
#include <z2/dop/DopGraph.h>
#include <z2/dop/DopTable.h>


namespace z2::dop {


DopPromise DopNode::get_input(int i, DopContext *visited) const {
    return graph->resolve_value(inputs[i].value, visited);
}


void DopNode::set_output(int i, std::any val) {
    outputs.at(i).result = std::move(val);
}


void DopNode::_apply_func(DopContext *visited) {
    auto func = tab.lookup(kind);
    func(this, visited);
}


DopPromise DopNode::get_output_by_name(std::string sock_name, DopContext *visited) {
    int n = -1;
    for (int i = 0; i < outputs.size(); i++) {
        if (outputs[i].name == sock_name) {
            n = i;
            break;
        }
    }
    if (n == -1)
        throw ztd::make_error("Bad output socket name: ", sock_name);

    _apply_func(visited);
    return visited->promise(this, n);
}


void DopNode::serialize(std::ostream &ss) const {
    ss << "DopNode[" << '\n';
    ss << "  name=" << name << '\n';
    ss << "  kind=" << kind << '\n';
    ss << "  inputs=[" << '\n';
    for (auto const &input: inputs) {
        ss << "    ";
        input.serialize(ss);
        ss << '\n';
    }
    ss << "  ]" << '\n';
    ss << "  outputs=[" << '\n';
    for (auto const &output: outputs) {
        ss << "    ";
        output.serialize(ss);
        ss << '\n';
    }
    ss << "  ]" << '\n';
    ss << "]" << '\n';
}


void DopNode::invalidate() {
    // todo: this func is called when param changed in editor
    // to supp fully sop, need invalidate its following nodes too
}


}  // namespace z2::dop
