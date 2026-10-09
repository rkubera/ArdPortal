// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdPortalFeatures.h"
#if ARDPORTAL_ENABLE_DEPENDENCIES
#include "ArdJSON.h"
#include "AppConfigFields.h"

namespace ArdDependencies {
using V = ArdJSON::JSONVar;
constexpr size_t MaxDepth = 6, MaxNodes = 64;
using Predicate = bool (*)(void*, const V&);

/**
 * @brief Validate a bounded condition tree and resolve every leaf identifier.
 * @param rule Leaf or nonempty and/or group to validate.
 * @param context Borrowed callback context; never allocated or retained.
 * @param resolve Callback accepting a leaf whose referenced field exists.
 * @param nodes In/out count of visited condition nodes.
 * @param depth Current group nesting depth; starts at zero.
 * @return True for a well-formed tree within its limits; false otherwise.
 */
inline __attribute__((noinline)) bool validateTree(const V& rule, void* context, Predicate resolve, size_t& nodes, size_t depth = 0) {
  if (depth > MaxDepth || ++nodes > MaxNodes || rule.type() != V::Type::Object) return false;
  const bool all = rule.hasOwnProperty("and"), any = rule.hasOwnProperty("or");
  if (all || any) {
    if (all == any || rule.length() != 1) return false;
    const V& children = rule[all ? "and" : "or"];
    if (children.type() != V::Type::Array || !children.length() || children.length() > MaxNodes) return false;
    for (size_t i = 0; i < children.length(); ++i)
      if (!validateTree(children[i], context, resolve, nodes, depth + 1)) return false;
    return true;
  }
  if (!rule.hasOwnProperty("field") || !rule.hasOwnProperty("equals") ||
      rule.length() != (rule.hasOwnProperty("property") ? 3U : 2U)) return false;
  const auto type = rule["equals"].type();
  if (type != V::Type::String && type != V::Type::Boolean && type != V::Type::Number && type != V::Type::Null) return false;
  return resolve(context, rule);
}

/**
 * @brief Evaluate a bounded condition tree with Boolean short-circuiting.
 * @param rule Condition tree; Undefined means no restriction.
 * @param context Borrowed callback context; never allocated or retained.
 * @param match Callback evaluating a leaf, including upstream visibility if required.
 * @param depth Current group nesting depth.
 * @return The Boolean result; excessive depth or invalid groups evaluate false.
 */
inline __attribute__((noinline)) bool evaluateTree(const V& rule, void* context, Predicate match, size_t depth = 0) {
  if (rule.isUndefined()) return true;
  if (depth > MaxDepth || rule.type() != V::Type::Object) return false;
  const bool all = rule.hasOwnProperty("and"), any = rule.hasOwnProperty("or");
  if (!all && !any) return match(context, rule);
  if (all == any) return false;
  const V& children = rule[all ? "and" : "or"];
  if (children.type() != V::Type::Array || !children.length() || children.length() > MaxNodes) return false;
  for (size_t i = 0; i < children.length(); ++i) {
    bool result = evaluateTree(children[i], context, match, depth + 1);
    if (all && !result) return false;
    if (any && result) return true;
  }
  return all;
}

/**
 * @brief Adapt a borrowed typed validator to the shared non-templated tree traversal.
 * @param rule Condition tree to validate.
 * @param resolve Leaf validator; invoked only during this call.
 * @param nodes In/out visited node count.
 * @return True if the condition is valid and within its resource limits.
 */
template<class Resolve>
bool validate(const V& rule, Resolve& resolve, size_t& nodes) {
  return validateTree(rule,&resolve,[](void* context,const V& leaf){return (*static_cast<Resolve*>(context))(leaf);},nodes);
}

/**
 * @brief Adapt a borrowed typed predicate to the shared Boolean tree evaluator.
 * @param rule Condition tree to evaluate.
 * @param match Leaf predicate; invoked only during this call.
 * @return Boolean result of the condition.
 */
template<class Match>
bool evaluate(const V& rule, Match& match) {
  return evaluateTree(rule,&match,[](void* context,const V& leaf){return (*static_cast<Match*>(context))(leaf);});
}

/**
 * @brief Locate a leaf in depth-first order without retaining its tree.
 * @param rule Condition tree to inspect.
 * @param ordinal In/out number of leaves to skip.
 * @param depth Current group nesting depth.
 * @return Pointer to the requested leaf, or nullptr at the end or depth limit.
 */
inline const V* leaf(const V& rule, size_t& ordinal, size_t depth = 0) {
  if (rule.isUndefined() || depth > MaxDepth) return nullptr;
  const bool group = rule.hasOwnProperty("and") || rule.hasOwnProperty("or");
  if (!group) { if (!ordinal) return &rule; --ordinal; return nullptr; }
  const V& children = rule[rule.hasOwnProperty("and") ? "and" : "or"];
  for (size_t i = 0; i < children.length(); ++i)
    if (const V* result = leaf(children[i], ordinal, depth + 1)) return result;
  return nullptr;
}

// Sparse reverse edges use small blocks rather than a 1024-by-1024 matrix.
class Edges {
  struct Edge {uint16_t controller,dependent;};
  struct Block {ARDPORTAL_NO_THROW_ALLOCATION Edge items[16];uint8_t used=0;std::unique_ptr<Block> next;};
  std::unique_ptr<Block> blocks;
public:
  /**
   * @brief Release sparse edge blocks iteratively without recursive destruction.
   * @return No value.
   */
  ~Edges(){while(blocks){auto next=std::move(blocks->next);blocks=std::move(next);}}
  /**
   * @brief Append a reverse dependency edge during atomic registration.
   * @param controller Index of the referenced field.
   * @param dependent Index of the field controlled by this reference.
   * @return True if stored; false on allocation failure.
   */
  bool add(size_t controller,size_t dependent){
    if(!blocks||blocks->used==16){std::unique_ptr<Block> block(new(std::nothrow) Block());if(!block)return false;block->next=std::move(blocks);blocks=std::move(block);}
    blocks->items[blocks->used++]={uint16_t(controller),uint16_t(dependent)};return true;
  }
  /**
   * @brief Build reverse edges for both field and inherited page conditions.
   * @param field Indexed field definition, including its inherited page condition.
   * @param dependent Destination field index.
   * @param resolve Callback resolving leaf field IDs to registered or staged indexes.
   * @return True if every unique controller was indexed; false on allocation failure.
   */
  template<class Resolve> bool addField(const V& field,size_t dependent,Resolve& resolve){
    ArdAppConfigFieldMask seen;
    for(const char* key:{"visibleWhen","_pageVisibleWhen"})for(size_t ordinal=0;;++ordinal){
      size_t skip=ordinal;const V* item=leaf(field[key],skip);if(!item)break;
      size_t controller=resolve((*item)["field"].asString());
      if(controller>=ArdAppConfigMaxFields)return false;
      if(!seen.test(controller)){if(!add(controller,dependent))return false;seen|=ArdAppConfigFieldMask::forField(controller);}
    }
    return true;
  }
  /**
   * @brief Collect immediate dependent fields for one controller without parsing JSON.
   * @param controller Changed field index.
   * @param result In/out dependent mask.
   * @return No value.
   */
  void dependents(size_t controller,ArdAppConfigFieldMask& result) const {
    for(const Block* b=blocks.get();b;b=b->next.get())for(size_t i=0;i<b->used;++i)
      if(b->items[i].controller==controller)result|=ArdAppConfigFieldMask::forField(b->items[i].dependent);
  }
  /**
   * @brief Mark all controller indexes represented by this edge list.
   * @param result In/out mask of registered controllers.
   * @return No value.
   */
  void controllers(ArdAppConfigFieldMask& result) const {
    for(const Block* b=blocks.get();b;b=b->next.get())for(size_t i=0;i<b->used;++i)result|=ArdAppConfigFieldMask::forField(b->items[i].controller);
  }
};

struct Frame { ARDPORTAL_NO_THROW_ALLOCATION  uint16_t index; uint8_t edge = 0, cursor = 0; };
// Allocate for graph depth rather than the total number of registered fields.
class Stack {
  std::unique_ptr<Frame[]> frames;
  size_t capacity=0;
public:
  /**
   * @brief Release the traversal buffer after completion or cancellation.
   * @return No value.
   */
  void reset(){frames.reset();capacity=0;}
  /**
   * @brief Check whether a traversal buffer has been allocated.
   * @return True when frames are available; false otherwise.
   */
  explicit operator bool() const {return bool(frames);}
  /**
   * @brief Access an allocated traversal frame.
   * @param index Frame position below the allocated capacity.
   * @return Reference to the frame.
   */
  Frame& operator[](size_t index){return frames[index];}
  /**
   * @brief Grow the buffer geometrically up to the field count, preserving active frames on failure.
   * @param required Number of frames needed.
   * @param limit Maximum possible graph depth, equal to the field count.
   * @return True if capacity is sufficient; false for an invalid size or allocation failure.
   */
  bool reserve(size_t required,size_t limit){
    if(required<=capacity)return true;
    if(required>limit)return false;
    size_t next=capacity?capacity*2:8;
    if(next<required)next=required;
    if(next>limit)next=limit;
    std::unique_ptr<Frame[]> replacement(new(std::nothrow) Frame[next]);
    if(!replacement)return false;
    for(size_t i=0;i<capacity;++i)replacement[i]=frames[i];
    frames=std::move(replacement);capacity=next;return true;
  }
};
enum class Result { Running, Visible, Hidden, Invalid, Cycle };

/**
 * @brief Advance one dependency graph edge or finish one field without recursive field traversal.
 * @param stack Caller-owned traversal buffer; grows only when another ancestor is needed.
 * @param depth In/out active frame count; caller starts with the root frame.
 * @param active In/out mask of ancestors for cycle detection.
 * @param complete In/out mask of evaluated fields.
 * @param visible In/out mask of fields whose conditions evaluated true.
 * @param count Number of fields available to the graph.
 * @param load Callback loading one indexed field definition.
 * @param indexOf Callback resolving a field identifier to its index.
 * @param dependent Callback checking whether a field has upstream conditions.
 * @param match Callback comparing one leaf against its current value.
 * @return Running until the root completes; visibility or a graph error afterwards.
 */
template<class Load, class Index, class Dependent, class Match>
Result step(Stack& stack, size_t& depth, ArdAppConfigFieldMask& active,
            ArdAppConfigFieldMask& complete, ArdAppConfigFieldMask& visible,
            size_t count, Load& load, Index& indexOf, Dependent& dependent, Match& match) {
  Frame& frame = stack[depth - 1];
  if (frame.edge >= 2) {
    const bool result = frame.edge == 2;
    auto bit = ArdAppConfigFieldMask::forField(frame.index);
    active &= ~bit; complete |= bit;
    if (result) visible |= bit;
    --depth;
    return depth ? Result::Running : (result ? Result::Visible : Result::Hidden);
  }
  const V field = load(frame.index);
  if (!field.isValid() || field.isUndefined()) return Result::Invalid;
  const V& rule = field[frame.edge ? "visibleWhen" : "_pageVisibleWhen"];
  size_t ordinal = frame.cursor;
  const V* reference = leaf(rule, ordinal);
  if (reference) {
    const size_t next = indexOf((*reference)["field"].asString());
    if (next >= count) return Result::Invalid;
    if (active.test(next)) return Result::Cycle;
    if (!complete.test(next) && !dependent(next)) {
      auto bit = ArdAppConfigFieldMask::forField(next);complete |= bit;visible |= bit;
    }
    if (!complete.test(next)) {
      if (depth >= count) return Result::Cycle;
      if (!stack.reserve(depth + 1, count)) return Result::Invalid;
      active |= ArdAppConfigFieldMask::forField(next);
      stack[depth++] = {uint16_t(next), 0, 0};
      return Result::Running;
    }
    ++frame.cursor;
    return Result::Running;
  }
  auto visibleMatch = [&](const V& item) {
    size_t index = indexOf(item["field"].asString());
    return index < count && visible.test(index) && match(item);
  };
  frame.edge = evaluate(rule, visibleMatch) ? uint8_t(frame.edge + 1) : 3;
  frame.cursor = 0;
  return Result::Running;
}
}
#endif
