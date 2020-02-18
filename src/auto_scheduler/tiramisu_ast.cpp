#include <tiramisu/auto_scheduler/ast.h>

namespace tiramisu::auto_scheduler
{
syntax_tree::syntax_tree(tiramisu::function *fct)
    : fct(fct)
{
    const std::vector<computation*> computations = fct->get_computations();
    
    for (computation *comp : computations) 
    {
        if (comp->get_expr().get_expr_type() == e_none)
            continue;

        ast_node *node = new ast_node(comp);
        node->parent = nullptr;
        
        roots.push_back(node);
    }
}

ast_node::ast_node(tiramisu::computation *comp)
{
    std::vector<ast_node*> nodes;

    // Get computation iterators
    isl_set *iter_domain = comp->get_iteration_domain();
    int nb_iterators = isl_set_dim(iter_domain, isl_dim_set);

    // The fist node is the one created by this constructor
    this->depth = 0;
    this->name = isl_set_get_dim_name(iter_domain, isl_dim_set, 0);
    this->low_bound = utility::get_bound(iter_domain, 0, false).get_int_val();
    this->up_bound = utility::get_bound(iter_domain, 0, true).get_int_val();

    nodes.push_back(this);
        
    // Create the other nodes, one for each iterator
    for (int i = 1; i < nb_iterators; ++i)
    {
        ast_node *node = new ast_node();
        
        node->depth = i;
        node->name = isl_set_get_dim_name(iter_domain, isl_dim_set, i);
        node->low_bound = utility::get_bound(iter_domain, i, false).get_int_val();
        node->up_bound = utility::get_bound(iter_domain, i, true).get_int_val();
        
        nodes.push_back(node);
    }

    // Chain the nodes together
    for (int i = 0; i < nodes.size() - 1; ++i)
        nodes[i]->children.push_back(nodes[i + 1]);

    nodes.back()->computations.push_back(comp);
}

syntax_tree* syntax_tree::copy_ast() const
{
    syntax_tree *ast = new syntax_tree();
    copy_and_return_node(*ast, nullptr);
    
    return ast;
}

ast_node* ast_node::copy_node() const
{
    ast_node *node = new ast_node();
    copy_and_return_node(node, nullptr);
    
    return node;
}

ast_node* syntax_tree::copy_and_return_node(syntax_tree& new_ast, ast_node *node_to_find) const
{
    ast_node *ret_node = nullptr;

    for (ast_node *root : roots) 
    {
        ast_node *new_node = new ast_node();

        ast_node *tmp = root->copy_and_return_node(new_node, node_to_find);
        if (tmp != nullptr)
            ret_node = tmp;

        new_node->parent = nullptr;
        new_ast.roots.push_back(new_node);
    }

    new_ast.fct = fct;
    new_ast.evaluation = evaluation;
    new_ast.search_depth = search_depth;
    new_ast.optims_info = optims_info;

    return ret_node;
}

ast_node* ast_node::copy_and_return_node(ast_node *new_node, ast_node *node_to_find) const
{
    ast_node *ret_node = nullptr;

    if (this == node_to_find)
        ret_node = new_node;

    for (ast_node *child : children)
    {
        ast_node *new_child = new ast_node();

        ast_node *tmp = child->copy_and_return_node(new_child, node_to_find);
        if (tmp != nullptr)
            ret_node = tmp;

        new_child->parent = new_node;
        new_node->children.push_back(new_child);
    }

    new_node->depth = depth;
    new_node->name = name;
    new_node->low_bound = low_bound;
    new_node->up_bound = up_bound;
    new_node->unrolled = unrolled;
    new_node->computations = computations;

    return ret_node;
}

void syntax_tree::transform_ast()
{
    if (optims_info.size() == 0)
        return ;
        
    optimization_info const& opt = optims_info.back();
    switch(opt.type)
    {
        case optimization_type::FUSION:
            transform_ast_by_fusion(opt);
            break;
            
        case optimization_type::TILING:
            transform_ast_by_tiling(opt);
            break;
            
        case optimization_type::INTERCHANGE:
            transform_ast_by_interchange(opt);
            break;
            
        case optimization_type::UNROLLING:
            transform_ast_by_unrolling(opt);
            break;
            
        default:
            break;
    }
}

void syntax_tree::transform_ast_by_fusion(optimization_info const& opt)
{
    std::vector<ast_node*> *tree_level;
    
    if (opt.node->parent != nullptr)
        tree_level = &opt.node->parent->children;
    else
        tree_level = &roots;
    
    ast_node *node1 = (*tree_level)[opt.l0];
    ast_node *node2 = (*tree_level)[opt.l1];

    for (ast_node *child : node2->children)
        node1->children.push_back(child);

    for (tiramisu::computation *comp : node2->computations)
        node1->computations.push_back(comp);

    tree_level->erase(tree_level->begin() + opt.l1);
}

void syntax_tree::transform_ast_by_tiling(optimization_info const& opt)
{
    ast_node *node = opt.node;
    
    if (opt.nb_l == 2)
    {
        // Create the new loop structure
        ast_node *i_outer = node;
        ast_node *j_outer = new ast_node();
            
        ast_node *i_inner = new ast_node();
        ast_node *j_inner = node->children[0];
            
        // Chain the nodes
        i_outer->children[0] = j_outer;
        j_outer->children.push_back(i_inner);
        i_inner->children.push_back(j_inner);
        
        j_outer->parent = i_outer;
        i_inner->parent = j_outer;
        j_inner->parent = i_inner;
            
        // Rename the nodes
        i_inner->name = i_outer->name + "_inner";
        i_outer->name = i_outer->name + "_outer";
            
        j_outer->name = j_inner->name + "_outer";
        j_inner->name = j_inner->name + "_inner";
            
        // Set lower and upper bounds
        i_outer->low_bound = 0;
        i_outer->up_bound = i_outer->get_extent() / opt.l0_fact - 1;
            
        j_outer->low_bound = 0;
        j_outer->up_bound = j_inner->get_extent() / opt.l1_fact - 1;
            
        i_inner->low_bound = 0;
        i_inner->up_bound = opt.l0_fact - 1;
            
        j_inner->low_bound = 0;
        j_inner->up_bound = opt.l1_fact - 1;
    }
        
    else if (opt.nb_l == 3)
    {
        // Create the new loop structure
        ast_node *i_outer = node;
        ast_node *j_outer = new ast_node();
        ast_node *k_outer = new ast_node();
            
        ast_node *i_inner = new ast_node();
        ast_node *j_inner = node->children[0];
        ast_node *k_inner = j_inner->children[0];
            
        // Chain the nodes
        i_outer->children[0] = j_outer;
        j_outer->children.push_back(k_outer);
        k_outer->children.push_back(i_inner);
        i_inner->children.push_back(j_inner);
        j_inner->children[0] = k_inner;
        
        j_outer->parent = i_outer;
        k_outer->parent = j_outer;
        i_inner->parent = k_outer;
        j_inner->parent = i_inner;
        k_inner->parent = j_inner;
            
        // Rename the nodes
        i_inner->name = i_outer->name + "_inner";
        i_outer->name = i_outer->name + "_outer";
            
        j_outer->name = j_inner->name + "_outer";
        j_inner->name = j_inner->name + "_inner";
            
        k_outer->name = k_inner->name + "_outer";
        k_inner->name = k_inner->name + "_inner";
            
        // Set lower and upper bounds
        i_outer->low_bound = 0;
        i_outer->up_bound = i_outer->get_extent() / opt.l0_fact - 1;
            
        j_outer->low_bound = 0;
        j_outer->up_bound = j_inner->get_extent() / opt.l1_fact - 1;
            
        k_outer->low_bound = 0;
        k_outer->up_bound = k_inner->get_extent() / opt.l2_fact - 1;
            
        i_inner->low_bound = 0;
        i_inner->up_bound = opt.l0_fact - 1;
            
        j_inner->low_bound = 0;
        j_inner->up_bound = opt.l1_fact - 1;
            
        k_inner->low_bound = 0;
        k_inner->up_bound = opt.l2_fact - 1;
    }

    node->update_depth(node->depth);
}

void syntax_tree::transform_ast_by_interchange(optimization_info const& opt)
{
    ast_node *node1 = opt.node;
    
    // Find the node to interchange with
    ast_node *node2 = node1;
    for (int i = opt.l0; i < opt.l1; ++i)
        node2 = node2->children[0];
            
    // Rename the two nodes
    std::string tmp_str;
    tmp_str = node1->name;
    node1->name = node2->name;
    node2->name = tmp_str;
            
    int tmp_int;
    tmp_int = node1->low_bound;
    node1->low_bound = node2->low_bound;
    node2->low_bound = tmp_int;
        
    tmp_int = node1->up_bound;
    node1->up_bound = node2->up_bound;
    node2->up_bound = tmp_int;
}

void syntax_tree::transform_ast_by_unrolling(optimization_info const& opt)
{
    ast_node *node = opt.node;
    
    if (node->get_extent() <= opt.l0_fact)
        node->unrolled = true;
        
    else 
    {
        // Create the new loop structure
        ast_node *i_outer = node;
        ast_node *i_inner = new ast_node();
        
        // Chain the nodes
        i_inner->computations = i_outer->computations;
        i_inner->children = i_outer->children;
        
        i_outer->computations.clear();
        i_outer->children.clear();
        i_outer->children.push_back(i_inner);
        
        i_inner->parent = i_outer;
        
        // Rename the nodes
        i_inner->name = i_outer->name + "_inner";
        i_outer->name = i_outer->name + "_outer";
        
        // Set lower and upper bounds
        i_outer->low_bound = 0;
        i_outer->up_bound = i_outer->get_extent() / opt.l0_fact - 1;
        
        i_inner->low_bound = 0;
        i_inner->up_bound = opt.l0_fact - 1;
        
        // Finalize unrolling
        i_inner->unrolled = true;
        i_inner->update_depth(i_outer->depth + 1);
    }
}

void ast_node::update_depth(int depth)
{
    this->depth = depth;
    
    for (ast_node *child : children)
        child->update_depth(depth + 1);
}

void ast_node::get_all_computations(std::vector<tiramisu::computation*>& comps)
{
    for (tiramisu::computation *c : computations)
        comps.push_back(c);
        
    for (ast_node *child : children)
        child->get_all_computations(comps);
}

int ast_node::get_loop_levels_chain_depth() const
{
    int ret = depth;
    const ast_node *node = this;
    
    while (node->children.size() == 1 && node->computations.size() == 0 && !node->unrolled)
    {
        ret++;
        node = node->children[0];
    }
    
    if ((node->children.size() == 0 || node->computations.size() > 0) && !node->unrolled)
        return ret + 1;
    
    return ret;
}

void ast_node::print_node() const
{
    for (int i = 0; i < depth; ++i)
        std::cout << "\t";

    std::cout << "for " << low_bound << " <= " << name << " < " << up_bound + 1 << " | " << unrolled << std::endl;
    
    for (tiramisu::computation* comp : computations) 
    {
        for (int i = 0; i < depth + 1; ++i)
            std::cout << "\t";
            
        std::cout << comp->get_name() << std::endl;
    }

    for (ast_node *child : children)
        child->print_node();
}

}