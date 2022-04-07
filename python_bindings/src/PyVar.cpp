#include "PyVar.h"

namespace tiramisu {
  namespace PythonBindings {

    void define_var(py::module &m){
      auto var_class = py::class_<var, expr>(m, "var")
        .def(py::init<tiramisu::primitive_t, std::string>())
        .def(py::init<std::string>())
        .def(py::init<std::string, tiramisu::expr, tiramisu::expr>())
        .def(py::init<>())
        .def("dump", [](const tiramisu::var &e) -> auto { return e.dump(true); });

      //TODO missing inits from tiramisu_expr.ccp
    }

  }  // namespace PythonBindings
}  // namespace tiramisu