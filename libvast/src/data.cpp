#include "vast/data.hpp"
#include "vast/json.hpp"

#include "vast/concept/printable/vast/data.hpp"
#include "vast/detail/assert.hpp"

namespace vast {
namespace {

struct adder {
  template <class T>
  std::enable_if_t<
    !(std::is_same<T, none>{}
      || std::is_same<T, vector>{}
      || std::is_same<T, set>{})
  >
  operator()(none, const T& x) const {
    self = x;
  }

  void operator()(none, none) const {
    // nop
  }

  template <class T>
  std::enable_if_t<
    !(std::is_same<T, vector>{} || std::is_same<T, set>{})
      && (std::is_same<T, boolean>{}
           || std::is_same<T, integer>{}
           || std::is_same<T, count>{}
           || std::is_same<T, real>{}
           || std::is_same<T, timespan>{}
           || std::is_same<T, std::string>{})
  >
  operator()(T& x, const T& y) const {
    x += y;
  }

  template <class T, class U>
  std::enable_if_t<
    !(std::is_same<T, U>{} || std::is_same<T, vector>{}
                           || std::is_same<T, set>{})
      && (std::is_same<T, boolean>{}
          || std::is_same<T, integer>{}
          || std::is_same<T, count>{}
          || std::is_same<T, real>{}
          || std::is_same<T, timespan>{}
          || std::is_same<T, std::string>{})
  >
  operator()(T&, const U&) const {
    // impossible
  }

  void operator()(timestamp&, timestamp) const {
    // impossible
  }

  void operator()(timestamp& ts, timespan x) const {
    ts += x;
  }

  template <class T>
  std::enable_if_t<
    !(std::is_same<T, timestamp>{}
      || std::is_same<T, timespan>{}
      || std::is_same<T, vector>{}
      || std::is_same<T, set>{})
  >
  operator()(timestamp&, const T&) const {
    // impossible
  }

  template <class T, class U>
  std::enable_if_t<
    !(std::is_same<U, vector>{} || std::is_same<U, set>{})
      && (std::is_same<T, pattern>{}
          || std::is_same<T, address>{}
          || std::is_same<T, subnet>{}
          || std::is_same<T, port>{}
          || std::is_same<T, enumeration>{}
          || std::is_same<T, table>{})
  >
  operator()(T&, const U&) const {
    // impossible
  }

  template <class T>
  std::enable_if_t<
    !(std::is_same<T, vector>{} || std::is_same<T, set>{})
  >
  operator()(vector& lhs, const T& rhs) const {
    lhs.emplace_back(rhs);
  }

  void operator()(vector& lhs, const vector& rhs) const {
    std::copy(rhs.begin(), rhs.end(), std::back_inserter(lhs));
  }

  template <class T>
  std::enable_if_t<
    !(std::is_same<T, vector>{} || std::is_same<T, set>{})
  >
  operator()(set& lhs, const T& rhs) const {
    lhs.emplace(rhs);
  }

  void operator()(set& lhs, const set& rhs) const {
    std::copy(rhs.begin(), rhs.end(), std::inserter(lhs, lhs.end()));
  }

  template <class T>
  std::enable_if_t<!std::is_same<T, vector>{}>
  operator()(T&, const vector& rhs) const {
    vector v;
    v.reserve(rhs.size() + 1);
    v.push_back(std::move(self));
    std::copy(rhs.begin(), rhs.end(), std::back_inserter(v));
    self = std::move(v);
  }

  template <class T>
  std::enable_if_t<!std::is_same<T, set>{}>
  operator()(T&, const set& rhs) const {
    set s;
    s.insert(std::move(self));
    std::copy(rhs.begin(), rhs.end(), std::inserter(s, s.end()));
    self = std::move(s);
  }

  data& self;
};

struct match_visitor {
  bool operator()(std::string const& lhs, pattern const& rhs) const {
    return rhs.match(lhs);
  }

  template <class T, class U>
  bool operator()(T const&, U const&) const {
    return false;
  }
};

struct in_visitor {
  bool operator()(std::string const& lhs, std::string const& rhs) const {
    return rhs.find(lhs) != std::string::npos;
  }

  bool operator()(std::string const& lhs, pattern const& rhs) const {
    return rhs.search(lhs);
  }

  bool operator()(address const& lhs, subnet const& rhs) const {
    return rhs.contains(lhs);
  }

  template <class T>
  bool operator()(T const& lhs, set const& rhs) const {
    return std::find(rhs.begin(), rhs.end(), lhs) != rhs.end();
  }

  template <class T>
  bool operator()(T const& lhs, vector const& rhs) const {
    return std::find(rhs.begin(), rhs.end(), lhs) != rhs.end();
  }

  template <class T, class U>
  bool operator()(T const&, U const&) const {
    return false;
  }
};

} // namespace <anonymous>

data::data(none) {
}

data& data::operator+=(data const& rhs) {
  visit(adder{*this}, *this, rhs);
  return *this;
}

detail::data_variant& expose(data& d) {
  return d.data_;
}

bool operator==(data const& lhs, data const& rhs) {
  return lhs.data_ == rhs.data_;
}

bool operator<(data const& lhs, data const& rhs) {
  return lhs.data_ < rhs.data_;
}

bool evaluate(data const& lhs, relational_operator op, data const& rhs) {
  switch (op) {
    default:
      VAST_ASSERT(!"missing case");
      return false;
    case match:
      return visit(match_visitor{}, lhs, rhs);
    case not_match:
      return !visit(match_visitor{}, lhs, rhs);
    case in:
      return visit(in_visitor{}, lhs, rhs);
    case not_in:
      return !visit(in_visitor{}, lhs, rhs);
    case ni:
      return visit(in_visitor{}, rhs, lhs);
    case not_ni:
      return !visit(in_visitor{}, rhs, lhs);
    case equal:
      return lhs == rhs;
    case not_equal:
      return lhs != rhs;
    case less:
      return lhs < rhs;
    case less_equal:
      return lhs <= rhs;
    case greater:
      return lhs > rhs;
    case greater_equal:
      return lhs >= rhs;
  }
}

data const* get(vector const& v, offset const& o) {
  vector const* x = &v;
  for (size_t i = 0; i < o.size(); ++i) {
    auto& idx = o[i];
    if (idx >= x->size())
      return nullptr;
    auto d = &(*x)[idx];
    if (i + 1 == o.size())
      return d;
    x = get_if<vector>(*d);
    if (!x)
      return nullptr;
  }
  return nullptr;
}

data const* get(data const& d, offset const& o) {
  if (auto v = get_if<vector>(d))
    return get(*v, o);
  return nullptr;
}

vector flatten(vector const& v) {
  vector result;
  result.reserve(v.size());
  for (auto& x : v)
    if (auto rec = get_if<vector>(x)) {
      auto flat = flatten(*rec);
      result.insert(result.end(),
                    std::make_move_iterator(flat.begin()),
                    std::make_move_iterator(flat.end()));
    } else {
      result.push_back(x);
    }
  return result;
}

data flatten(data const& d) {
  auto v = get_if<vector>(d);
  return v ? flatten(*v) : d;
}

namespace {

template <class Iterator>
optional<vector> unflatten(Iterator& f, Iterator l, const record_type& rec) {
  vector xs;
  xs.reserve(rec.fields.size());
  for (auto& field : rec.fields)
    if (auto r = get_if<record_type>(field.type)) {
      auto ys = unflatten(f, l, *r);
      if (!ys)
        return ys;
      xs.push_back(std::move(*ys));
    } else if (f != l) {
      xs.push_back(*f++);
    } else {
      return {};
    }
  return xs;
}

} // namespace <anonymous>

optional<vector> unflatten(vector const& v, record_type const& t) {
  auto first = v.begin();
  auto last = v.end();
  return unflatten(first, last, t);
}

optional<vector> unflatten(data const& d, type const& t) {
  auto v = get_if<vector>(d);
  auto rt = get_if<record_type>(t);
  return v && rt ? unflatten(*v, *rt) : optional<vector>{};
}

namespace {

struct jsonizer {
  jsonizer(json& j) : j_{j} { }

  bool operator()(none) const {
    return true;
  }

  bool operator()(std::string const& str) const {
    j_ = str;
    return true;
  }

  template <class T>
  bool operator()(T const& x) const {
    return convert(x, j_);
  }

  json& j_;
};

} // namespace <anonymous>

bool convert(vector const& v, json& j) {
  json::array a(v.size());
  for (auto i = 0u; i < v.size(); ++i)
    if (!visit(jsonizer{a[i]}, v[i]))
      return false;
  j = std::move(a);
  return true;
}

bool convert(set const& s, json& j) {
  json::array a(s.size());
  auto i = 0u;
  for (auto& x : s)
    if (!visit(jsonizer{a[i++]}, x))
      return false;
  j = std::move(a);
  return true;
}

bool convert(table const& t, json& j) {
  json::array values;
  for (auto& p : t) {
    json::array a;
    json j;
    if (!visit(jsonizer{j}, p.first))
      return false;
    a.push_back(std::move(j));
    if (!visit(jsonizer{j}, p.second))
      return false;
    a.push_back(std::move(j));
    values.emplace_back(std::move(a));
  };
  j = std::move(values);
  return true;
}

bool convert(data const& d, json& j) {
  return visit(jsonizer{j}, d);
}

bool convert(data const& d, json& j, type const& t) {
  auto v = get_if<vector>(d);
  auto rt = get_if<record_type>(t);
  if (v && rt) {
    if (v->size() != rt->fields.size())
      return false;
    json::object o;
    for (auto i = 0u; i < v->size(); ++i) {
      auto& f = rt->fields[i];
      if (! convert((*v)[i], o[f.name], f.type))
        return false;
    }
    j = std::move(o);
    return true;
  }
  return convert(d, j);
}

} // namespace vast
