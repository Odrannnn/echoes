#ifndef _RSTL_RED_BLACK_TREE
#define _RSTL_RED_BLACK_TREE

#include "types.h"

#include "rstl/functional.hpp"
#include "rstl/iterator.hpp"
#include "rstl/pair.hpp"
#include "rstl/rmemory_allocator.hpp"

namespace rstl {

enum node_color {
  kNC_Black,
  kNC_Red,
};

void rbtree_rebalance(void*, void*);
void* rbtree_traverse_forward(const void*, void*);
void* rbtree_rebalance_for_erase(void* header, void* node_void);

template < typename T, typename P, int U, typename S = select1st< P >, typename Cmp = less< T >,
           typename Alloc = rmemory_allocator >
class red_black_tree {
private:
  struct node {
    node* mLeft;
    node* mRight;
    node* mParent;
    node_color mColor;
    uchar mValue[sizeof(P)];

    node(node* left, node* right, node* parent, node_color color, const P& value)
    : mLeft(left), mRight(right), mParent(parent), mColor(color) {
      construct(get_value(), value);
    }
    ~node() { get_value()->~P(); }

    P* get_value() { return reinterpret_cast< P* >(&mValue); }
    const P* get_value() const { return reinterpret_cast< const P* >(&mValue); }

    node* get_left() { return mLeft; }
    void set_left(node* n) { mLeft = n; }
    node* get_right() { return mRight; }
    void set_right(node* n) { mRight = n; }
  };
  class header {
  public:
    header() : mLeftmost(nullptr), mRightmost(nullptr), mRootNode(nullptr) {}
    void set_root(node* n) { mRootNode = n; }
    void set_leftmost(node* n) { mLeftmost = n; }
    void set_rightmost(node* n) { mRightmost = n; }

    node* get_root() const { return mRootNode; }
    node* get_leftmost() const { return mLeftmost; }
    node* get_rightmost() const { return mRightmost; }

  private:
    node* mLeftmost;
    node* mRightmost;
    node* mRootNode;
  };

public:
  struct const_iterator {
    typedef int difference_type;
    typedef forward_iterator_tag iterator_category;
    typedef P* value_type;

    node* mNode;
    const header* mHeader;
    // bool x8_;

    // TODO why is this bool here?
    const_iterator(node* node, const header* header, bool b)
    : mNode(node), mHeader(header) /*, x8_(b)*/ {}

    const P* operator->() const { return mNode->get_value(); }
    const P* operator*() const { return mNode->get_value(); }
    bool operator==(const const_iterator& other) const {
      return mNode == other.mNode && mHeader == other.mHeader;
    }
    bool operator!=(const const_iterator& other) const {
      // return !(*this == other);
      return mNode != other.mNode || mHeader != other.mHeader;
    }

    const_iterator& operator++() {
      mNode = static_cast< node* >(rbtree_traverse_forward(static_cast< const void* >(mHeader),
                                                           static_cast< void* >(mNode)));
      return *this;
    }

    const_iterator operator++(int) {
      const_iterator result = *this;
      mNode = static_cast< node* >(rbtree_traverse_forward(static_cast< const void* >(mHeader),
                                                           static_cast< void* >(mNode)));
      return result;
    }
  };
  struct iterator : public const_iterator {
    iterator(node* node, const header* header, bool b) : const_iterator(node, header, b) {}

    P* operator->() { return const_iterator::mNode->get_value(); }
    P* operator*() { return const_iterator::mNode->get_value(); }
    node* get_node() { return const_iterator::mNode; }
  };

  red_black_tree() : x0_(0), x1_(0), x4_count(0) {}
  ~red_black_tree() { clear(); }

  iterator insert_into(node* n, const P& item);
  iterator insert(const P& item) { return insert_into(x8_header.get_root(), item); }

  const_iterator begin() const {
    // TODO
    return const_iterator(x8_header.get_leftmost(), &x8_header, false);
  }
  const_iterator end() const {
    // TODO
    return const_iterator(nullptr, &x8_header, false);
  }

  iterator begin() {
    // TODO
    return iterator(x8_header.get_leftmost(), &x8_header, false);
  }
  iterator end() {
    // TODO
    return iterator(nullptr, &x8_header, false);
  }

  const_iterator find(const T& key) const {
    node* n = x8_header.get_root();
    node* needle = nullptr;
    while (n != nullptr) {
      if (!x2_cmp(x3_selector(*n->get_value()), key)) {
        needle = n;
        n = n->get_left();
      } else {
        n = n->get_right();
      }
    }
    bool noResult = false;
    if (needle == nullptr || x2_cmp(key, x3_selector(*needle->get_value()))) {
      noResult = true;
    }
    if (noResult) {
      needle = nullptr;
    }
    return const_iterator(needle, &x8_header, false);
  }

  iterator find(const T& key) {
    node* n = x8_header.get_root();
    node* needle = nullptr;
    while (n != nullptr) {
      if (!x2_cmp(x3_selector(*n->get_value()), key)) {
        needle = n;
        n = n->get_left();
      } else {
        n = n->get_right();
      }
    }
    bool noResult = false;
    if (needle == nullptr || x2_cmp(key, x3_selector(*needle->get_value()))) {
      noResult = true;
    }
    if (noResult) {
      needle = nullptr;
    }
    return iterator(needle, &x8_header, false);
  }

  iterator erase(iterator it) {
    node* node = it.get_node();
    ++it;
    free_node(rebalance_for_erase(node));
    x4_count--;
    return it;
  }

  void clear() {
    node* root = x8_header.get_root();
    if (root != nullptr) {
      free_node_and_sub_nodes(root);
    }
    x8_header.set_root(nullptr);
    x8_header.set_leftmost(nullptr);
    x8_header.set_rightmost(nullptr);
    x4_count = 0;
  }

  int size() const { return x4_count; }

private:
  uchar x0_;
  uchar x1_;
  Cmp x2_cmp;
  S x3_selector;
  int x4_count;
  header x8_header;

  node* create_node(node* left, node* right, node* parent, node_color color, const P& value) {
    node* n;
    Alloc::allocate(n, 1);
    new (n) node(left, right, parent, color, value);
    return n;
  }

  void free_node_and_sub_nodes(node* n) {
    if (node* left = n->get_left()) {
      free_node_and_sub_nodes(left);
    }
    if (node* right = n->get_right()) {
      free_node_and_sub_nodes(right);
    }
    free_node(n);
  }

  void free_node(node* n) {
    n->~node();
    Alloc::deallocate(n);
  }

  void rebalance(node* n) { rbtree_rebalance(&x8_header, n); }

  node* rebalance_for_erase(node* n) {
    return static_cast< node* >(rbtree_rebalance_for_erase(&x8_header, n));
  }
};

static const bool kUnknownValueNewRoot = true;
static const bool kUnknownValueEqualKey = false;
static const bool kUnknownValueNewItem = true;

template < typename T, typename P, int U, typename S, typename Cmp, typename Alloc >
typename red_black_tree< T, P, U, S, Cmp, Alloc >::iterator
red_black_tree< T, P, U, S, Cmp, Alloc >::insert_into(node* n, const P& item) {
  if (n == nullptr) {
    // **The root is BLACK, and that is not a stylistic choice - it is forced by
    // `rbtree_rebalance`, which is retail's own byte-exact code.** That function's loop is
    //
    //     while (node->mParent != nullptr && node->mParent->mColor == kNC_Red) {
    //       fake_node* p = node->mParent->mParent->mLeft;   // <- unguarded
    //
    // so it dereferences `mParent->mParent` **without checking it**. A red root therefore makes
    // the *second* insert into an empty tree fault: the new node's parent is the root, the root
    // is red, the loop is entered, and `root->mParent` is null. The boot did exactly that, on
    // the first `factoryMgr.RegisterFactoryByTypeIdx('STRG', ...)` of step 13 - a null deref at
    // `mov (%rdx),%rax` in `rstl::rbtree_rebalance`.
    //
    // Retail cannot have this bug, because retail's `rbtree_rebalance` is Matching at 100.00%
    // and 8 of the 12 bytes of `node` are the colour and parent it reads. **So the root is
    // black in retail and red here**, and the invariant "a root is black" is the same one the
    // red-red fix-up is defined against.
    //
    // `insert_into` is a template member, so it is **host-only reimplementation code** and not
    // one of the five functions `main/rstl/rstl_map` claims - which is why this change is a
    // no-op for the DOL. Verified by the hash, not by argument.
    x8_header.set_root(create_node(nullptr, nullptr, nullptr, kNC_Black, item));
    x4_count += 1;
    x8_header.set_leftmost(x8_header.get_root());
    x8_header.set_rightmost(x8_header.get_root());
    return iterator(x8_header.get_root(), &x8_header, kUnknownValueNewRoot);

  } else {
    // **`firstComp` is `item < node`, and a new leaf is RED on both sides.** This loop used to
    // compute `node < item` and go *left* on it, which builds the tree in descending order, while
    // `find` below and retail's `rbtree_traverse_forward` (Matching, rstl_map.cpp) both assume
    // ascending: `find` goes left when `!(node < key)`, and the successor is the leftmost node
    // of the right subtree. Measured with a host harness that inserts twelve FourCCs into an
    // `rstl::map<uint, int>` and then looks each one up - the lookup `CFactoryMgr` and
    // `CSimplePool` do by type:
    //
    //   before: found 0/12; iteration order: 1 0 7 4 8 5 6 11 10 2 3 9
    //   after:  found 12/12; iteration order is ascending by key
    //
    // Every `find` missed, including the root's key. The new right leaf was also created
    // `kNC_Black`, which breaks the black-height invariant `rbtree_rebalance` fixes up against -
    // that function only repairs red-red violations, so a black leaf is never repaired. The
    // sibling Prime 1 port's header (`MetroidPrimePort/include/rstl/red_black_tree.hpp`) has
    // `x1_cmp(item, node)` and `kNC_Red` for both leaves, which is this.
    //
    // `U` is the tree's multi flag (`multimap` passes 1): only a unique tree returns the existing
    // node for an equal key. A multimap that returned it would silently drop the insert.
    //
    // Host-only, like the root colour above: `insert_into` is a template member that no
    // `Matching` unit instantiates, so this cannot move `main.dol`. Verified by the hash.
    node* newNode = nullptr;
    while (newNode == nullptr) {
      bool firstComp = x2_cmp(x3_selector(item), x3_selector(*n->get_value()));
      if (!U && !firstComp && !x2_cmp(x3_selector(*n->get_value()), x3_selector(item))) {
        return iterator(n, &x8_header, kUnknownValueEqualKey);
      }
      if (firstComp) {
        if (n->get_left() == nullptr) {
          newNode = create_node(nullptr, nullptr, n, kNC_Red, item);
          n->set_left(newNode);
          if (n == x8_header.get_leftmost()) {
            x8_header.set_leftmost(newNode);
          }
        } else {
          n = n->get_left();
        }
      } else {
        if (n->get_right() == nullptr) {
          newNode = create_node(nullptr, nullptr, n, kNC_Red, item);
          n->set_right(newNode);
          if (n == x8_header.get_rightmost()) {
            x8_header.set_rightmost(newNode);
          }
        } else {
          n = n->get_right();
        }
      }
    }
    x4_count += 1;
    rebalance(newNode);
    return iterator(newNode, &x8_header, kUnknownValueNewItem);
  }
}

}; // namespace rstl

#endif // _RSTL_RED_BLACK_TREE
