// RUN: %check_clang_tidy %s misc-no-implicit-conversions %t


template<typename T>
struct Holder
{
  template<typename Other>
  Holder(const Holder<Other> &);

  Holder() = default;
};

struct Base { };
struct Derived : Base {};

template<typename Char>
struct basic_string_view
{
  basic_string_view(const Char *);
};

using string_view = basic_string_view<char>;

template<typename Char>
struct basic_string
{
  basic_string(const Char *);
  basic_string();
  const Char *c_str() const;
  ~basic_string();
  operator basic_string_view<Char>() const;
};

using string = basic_string<char>;


struct istream {
  explicit operator bool() const;
  ~istream();
  bool good() const;
};

istream &getline(istream &input, string &output);


void implicit_holder_conversion(const Holder<Base> &);

void get_implicit_holder_conversion() {
  auto derived = Holder<Derived>{};
  implicit_holder_conversion(derived);
  // CHECK-MESSAGES: :[[@LINE-1]]:30: warning: implicit conversion from 'Holder<Derived>' to 'Holder<Base>' creates new object [misc-no-implicit-conversions]

  implicit_holder_conversion(Holder<Derived>());
  // CHECK-MESSAGES: :[[@LINE-1]]:30: warning: implicit conversion from 'Holder<Derived>' to 'Holder<Base>' creates new object [misc-no-implicit-conversions]

  // Should not warn
  implicit_holder_conversion(Holder<Base>());
}


void implicit_inheritance_conversion(const Base &);
void implicit_inheritance_conversion(const Base *);

void get_implicit_inheritance_conversion() {
  // non of these should warn

  auto derived = Derived{};
  implicit_inheritance_conversion(&derived);
  implicit_inheritance_conversion(Derived{});
  implicit_inheritance_conversion(derived);
}


void implicit_slicing_conversion(Base);

void set_implicit_slicing_conversion()
{
  auto derived = Derived{};

  implicit_slicing_conversion(derived);
  // CHECK-MESSAGES: :[[@LINE-1]]:31: warning: implicit conversion from 'Derived' to 'Base' causes slicing and creates new object [misc-no-implicit-conversions]

  Base b = derived;
  // CHECK-MESSAGES: :[[@LINE-1]]:12: warning: implicit conversion from 'Derived' to 'Base' causes slicing and creates new object [misc-no-implicit-conversions]

  implicit_slicing_conversion(Derived{});
  // CHECK-MESSAGES: :[[@LINE-1]]:31: warning: implicit conversion from 'Derived' to 'Base' causes slicing and creates new object [misc-no-implicit-conversions]

  // should not warn
  implicit_slicing_conversion(Base{});
}

void implicit_0_to_nullptr() {
  implicit_inheritance_conversion(0);
  // CHECK-MESSAGES: :[[@LINE-1]]:35: warning: implicit conversion from 'int' to 'const Base *', use nullptr instead [misc-no-implicit-conversions]

  // we'll allow this one, it's what nullptr is designed for... but we'll reevaluate in the future
  implicit_inheritance_conversion(nullptr);
}

string get_string(const string &s) {
  return s.c_str();
  // CHECK-MESSAGES: :[[@LINE-1]]:10: warning: implicit conversion from 'const char *' to 'string' (aka 'basic_string<char>') creates new non-trivial object [misc-no-implicit-conversions]
}

void implicit_to_bool()
{
  istream input;
  string output;

  while (getline(input, output)) {
    // CHECK-MESSAGES: :[[@LINE-1]]:10: warning: implicit conversion from 'istream' to 'bool' affects readability [misc-no-implicit-conversions]
  }

  while (getline(input, output).good()) {
    // should not error
  }
}

string_view get_value()
{
  string mystring;
  return mystring;
  // CHECK-MESSAGES: :[[@LINE-1]]:10: warning: implicit conversion from 'basic_string<char>' to 'basic_string_view<char>', potential for dangling reference detected [misc-no-implicit-conversions]
}

void promotions() {
  const auto double_func = [](double){};
  double_func(1.3f);
  // CHECK-MESSAGES: :[[@LINE-1]]:15: warning: implicit conversion from 'float' to 'double' affects overload resolution, and might change meaning if more overloads are added [misc-no-implicit-conversions]
  double_func(1);
  // CHECK-MESSAGES: :[[@LINE-1]]:15: warning: implicit conversion from 'int' to 'double' affects overload resolution, and might change meaning if more overloads are added [misc-no-implicit-conversions]

  const auto long_long_func = [](long long){};
  long_long_func(1);
  // CHECK-MESSAGES :[[@LINE-1]]:18: warning: implicit conversion from 'int' to 'long long' affects overload resolution, and might change meaning if more overloads are added [misc-no-implicit-conversions]
}
