/* indivudal.cpp - CS 472 Project #3: Genetic Programming
 * Copyright 2014 Andrew Schwartzmeyer
 *
 * Source file for individual namespace
 */

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <tuple>
#include <vector>

#include "individual.hpp"
#include "../options/options.hpp"
#include "../random_generator/random_generator.hpp"

namespace individual
{
  using std::vector;
  using std::string;
  using namespace random_generator;

  // Default Size struct constructor.
  Size::Size(): internals{0}, leaves{0}, depth{0} {}

  using F = Function;
  // Vectors of same-arity function enums.
  vector<F> nullaries { F::left, F::right, F::forward };
  vector<F> binaries { F::prog2, F::iffoodahead };
  vector<F> trinaries { F::prog3 };

  /* Vectors of available function enums.  Should be moved into
     Options struct. */
  vector<F> leaves = nullaries;
  vector<F> internals { F::prog2, F::prog3, F::iffoodahead };

  // Returns a random function from a given set of functions.
  Function
  get_function(const vector<Function>& functions)
  {
    size_dist dist{0, functions.size() - 1}; // closed interval
    return functions[dist(rg.engine)];
  }

  // Returns bool of whether or not the item is in the set.
  template<typename I, typename S> bool
  contains(const I& item, const S& set)
  { return find(begin(set), end(set), item) != end(set); }

  // Returns the appropriate arity for a given function.
  int
  get_arity(const Function function)
  {
    if (contains(function, nullaries))
      { return 0; }
    else if (contains(function, binaries))
      { return 2; }
    else if (contains(function, trinaries))
      { return 3; }
    assert(false);
  }

  // Default constructor for "empty" node
  Node::Node(): function{Function::nil}, arity{0} {}

  // Delegate that unpacks a tuple as args to actual constructor
  Node::Node(std::tuple<Method, int, int> args):
    Node{std::get<0>(args), std::get<1>(args), std::get<2>(args)} {}

  /* Recursively constructs a parse tree using the given method
     (either 'grow' or 'full'). */
  Node::Node(Method method, int max_depth, int depth): Node{}
  {
    // Create leaf node if at the max depth or randomly (if growing).
    float chance =
      static_cast<float>(leaves.size()) / (leaves.size() + internals.size());
    bool_dist dist(chance);
    if (depth == max_depth
	or (depth != 0 and (method == Method::grow and dist(rg.engine))))
      { function = get_function(leaves); }
    else // Otherwise choose an internal node.
      {
	function = get_function(internals);
	arity = get_arity(function);
	// Recursively create subtrees.
	children.reserve(arity);
	generate_n(back_inserter(children), arity, [method, max_depth, depth]
		   { return Node{method, max_depth, depth + 1}; });
      }
    assert(function != Function::nil); // do not create null types
    assert(children.size() == arity); // ensure arity
  }

  /* Return a grown node using a random method and depth from provided
     range.  The min_depth and max_depth are casted to int in the
     arguments to eliminate need for static_cast<int> in the
     depth_dist. */
  std::tuple<Method, int, int>
  get_node_args(int min = 0, int max = 0, float chance = 0.5)
  {
    bool_dist method_dist{chance};
    const Method method = (method_dist(rg.engine))
      ? Method::grow : Method::full;

    int_dist depth_dist{min, max};
    const int depth = depth_dist(rg.engine);

    return std::make_tuple(method, depth, 0); // 0 is starting depth
  }

  // Returns a string visually representing a particular node.
  string
  Node::represent() const
  {
    switch (function)
      {
      case F::nil:
	{ assert(false); } // Never represent empty node.

      case F::prog2:
	{ return "prog-2"; }

      case F::prog3:
	{ return "prog-3"; }

      case F::iffoodahead:
	{ return "if-food-ahead"; }

      case F::left:
	{ return "left"; }

      case F::right:
	{ return "right"; }

      case F::forward:
	{ return "forward"; }
      }
    assert(false); // Every node should have been matched.
  }

  /* Returns string representation of expression in Polish/prefix
     notation using a pre-order traversal. */
  string
  Node::print() const
  {
    if (children.empty())
      { return represent(); }

    string formula = "(" + represent();

    for (const Node& child : children)
      { formula += " " + child.print(); }

    return formula + ")";
  }

  /* Evaluates an ant over a given map using a depth-first post-order
     recursive continuous evaluation of a decision tree. */
  void
  Node::evaluate(options::Map& map) const
  {
    if (not map.active()) return;

    switch (function)
      {
      case F::left:
	{ map.left(); break; } // Terminal case

      case F::right:
	{ map.right(); break; } // Terminal case

      case F::forward:
	{ map.forward(); break; } // Terminal case

      case F::iffoodahead:
	{
	  if (map.look()) // Do left or right depending on if food ahead
	    { begin(children)->evaluate(map); }
	  else
	    { next(begin(children))->evaluate(map); }
	  break;
	}

      case F::prog2: // Falls through
      case F::prog3:
	{
	  for (const auto& child : children)
	    { child.evaluate(map); }
	  break;
	}

      case F::nil:
	{ assert(false); } // Never evaluate empty node
      }
  }

  /* Get size of node. */
  const Size Node::size() const
  {
    Size s;
    size(s);
    return s;
  }

  /* Recursively count children and find maximum depth of tree via
     depth-first traversal.  Keep track of internals, leaves, and depth
     via Size struct sent by reference. */
  void
  Node::size(Size& s) const
  {
    if (children.empty())
      {
	++s.leaves; // Count as leaf node
	s.depth = 0; // Reset the depth to zero
      }
    else
      {
	vector<int> depths;
	depths.reserve(arity);
	for (const auto& child : children)
	  {
	    child.size(s); // Recursively call size()
	    depths.push_back(s.depth); // Save depths
	  }

	++s.internals; // Count as internal node
	s.depth = 1 + *max_element(begin(depths), end(depths)); // Save max depth
      }
  }

  // Used to represent "not-found" (similar to a NULL pointer).
  Node empty;

  /* Depth-first search for taget node.  Must be seeking either
     internal or leaf, cannot be both. */
  Node&
  Node::visit(const Size& i, Size& visiting)
  {
    for (auto& child : children)
      {
	// Increase relevant count.
	if (child.children.empty())
	  { ++visiting.leaves; }
	else
	  { ++visiting.internals; }

	// Return node reference if found.
	if (visiting.internals == i.internals or visiting.leaves == i.leaves)
	  { return child; }

	Node& temp = child.visit(i, visiting); // Recursive search.
	if (temp.function != Function::nil)
	  { return temp; }
      }
    return empty;
  }

  void
  Node::mutate(int min, int max, float chance)
  {
    if (arity == 0)
      {
	const Function old = function;
	while (function == old)
	  function = get_function(leaves);
      }
    else if (arity == 2 or arity == 3)
      {
	const Function old = function;
	while (function == old)
	  function = get_function(internals);
	arity = get_arity(function);
      }
    else
      { assert(false); }

    // Fix arity mismatches caused by mutation
    while (children.size() > arity)
      { children.pop_back(); }

    while (children.size() < arity)
      { children.emplace_back(get_node_args(min, max, chance)); }

    assert(arity == children.size());
    assert(function != Function::nil);
  }

  // Default constructor for Individual
  Individual::Individual() {}

  /* Create an Individual tree by having a root node (to which the
     actual construction is delegated).  Calling evaluate updates the
     size, fitness, adjusted fitness, and score. */
  Individual::Individual(const options::Options& options)
    : root{get_node_args(options.min_depth, options.max_depth, options.grow_chance)},
      score{0}, fitness{0}, adjusted{0}
  { evaluate(options.map); }

  // Return string representation of a tree's size and fitness.
  string
  Individual::print() const
  {
    using std::to_string;

    string info = "# Size " + to_string(get_total()) + ", with "
      + to_string(get_internals()) + " internals, "
      + to_string(get_leaves()) + " leaves, and depth of "
      + to_string(get_depth()) + ".\n"
      + "# Score: " + to_string(score)
      + ", and adjusted fitness: " + to_string(adjusted) + ".\n";

    return info;
  }

  // Return string represenation of tree's expression (delegated).
  string
  Individual::print_formula() const
  { return "# Formula: " + root.print() + "\n"; }

  /* Evaluate Individual for given values and calculate size.  Update
     Individual's size and fitness accordingly. Return non-empty
     string if printing. */
  string
  Individual::evaluate(options::Map map, float penalty, bool print)
  {
    // Update size on evaluation because it's incredibly convenient.
    size = root.size();

    // Run ant across map and retrieve fitness.
    while (map.active())
      { root.evaluate(map); }
    score = map.fitness();

    // Adjusted fitness does not have size penalty.
    adjusted = static_cast<float>(score) / map.max();

    // Apply size penalty.
    fitness = score - penalty * get_total();

    string evaluation;
    if (print)
      { evaluation = map.print(); }

    return evaluation;
  }

  using O = Operator;
  // Vectors of same-arity function enums.
  vector<O> operators {O::shrink, O::hoist, O::subtree, O::replacement};


  // Mutate each node with given probability.
  void
  Individual::mutate(int min, int max, float chance)
  {
    size_dist op_dist{0, operators.size() - 1}; // closed interval
    const Operator op = operators[op_dist(rg.engine)];

    Size p = get_node_location(Type::internal);
    if (at(p).children.empty()) return; // p may have been root

    size_dist c_dist{0, at(p).children.size() - 1}; // closed interval
    const unsigned int c = c_dist(rg.engine);
    assert(c <= at(p).arity);

    switch (op)
      {
      case O::shrink:
	// Replace c with a leaf node
	at(p).children[c] = std::move(Node{get_node_args(0, 0)}); break;

      case O::hoist:
	// Make c the new root
	std::swap(root, at(p).children[c]); break;

      case O::subtree:
	// Replace c with new subtree to depth 6
	at(p).children[c] = std::move(Node{get_node_args(min, max, chance)}); break;

      case O::replacement:
	// Replace c with node of same type (internal/leaf)
	at(p).children[c].mutate(min, max, chance); break;
      }
  }

  // Safely return reference to desired node.
  Node&
  Individual::operator[](const Size& i)
  {
    assert(i.internals <= static_cast<unsigned int>(get_internals()));
    assert(i.leaves <= static_cast<unsigned int>(get_leaves()));

    Size visiting;
    // Return root node if that's what we're seeking.
    if (i.internals == 0 and i.leaves == 0)
      return root;
    else
      return root.visit(i, visiting);
  }

  Node&
  Individual::at(const Size& i)
  { return operator[](i); }

  Size
  Individual::get_node_location(Type type) const
  {
    Size target;
    // Guaranteed to have at least 1 leaf, but may have 0 internals.
    if (type == Type::internal and get_internals() != 0)
      {
	// Choose an internal node.
	int_dist dist{0, get_internals() - 1};
	target.internals = dist(rg.engine);
      }
    else
      {
	// Otherwise choose a leaf node.
	int_dist dist{0, get_leaves() - 1};
	target.leaves = dist(rg.engine);
      }
    return target;
  }

  /* Swap two random subtrees between Individuals "a" and "b",
     selecting an internal node with chance probability. */
  void
  crossover(const float chance, Individual& a, Individual& b)
  {
    bool_dist dist(chance);

    Individual::Type type_a = (dist(rg.engine))
      ? Individual::Type::internal : Individual::Type::leaf;

    Individual::Type type_b = (dist(rg.engine))
      ? Individual::Type::internal : Individual::Type::leaf;

    std::swap(a[a.get_node_location(type_a)], b[b.get_node_location(type_b)]);
  }

  // Read-only "getters" for private data

  int
  Individual::get_internals() const
  { return size.internals; }

  int
  Individual::get_leaves() const
  { return size.leaves; }

  int
  Individual::get_total() const
  { return size.internals + size.leaves; }

  int
  Individual::get_depth() const
  { return size.depth; }

  int
  Individual::get_score() const
  { return score; }

  float
  Individual::get_fitness() const
  { return fitness; }

  float
  Individual::get_adjusted() const
  { return adjusted; }
}
