/******************************************************************************
 *                    _   _____   __________                                  *
 *                   | | / / _ | / __/_  __/     Visibility                   *
 *                   | |/ / __ |_\ \  / /          Across                     *
 *                   |___/_/ |_/___/ /_/       Space and Time                 *
 *                                                                            *
 * This file is part of VAST. It is subject to the license terms in the       *
 * LICENSE file found in the top-level directory of this distribution and at  *
 * http://vast.io/license. No part of VAST, including this file, may be       *
 * copied, modified, propagated, or distributed except according to the terms *
 * contained in the LICENSE file.                                             *
 ******************************************************************************/

#pragma once

#include <string_view>

#include <caf/fwd.hpp>
#include <caf/message.hpp>

#include "vast/command.hpp"

namespace vast::system {

class application {
public:
  /// Constructs an application.
  application();

  /// Adds a new command to the application.
  template <class Options>
  auto add(command::fun run, std::string_view name,
           std::string_view description, Options&& options) {
    return root.add(run, name, description, std::forward<Options>(options));
  }

  /// Starts the application and blocks until execution completes.
  /// @returns the type-erased result of the executed command or a wrapped
  ///          `caf::error`.
  auto run(caf::actor_system& sys, command::argument_iterator first,
           command::argument_iterator last) {
    return vast::run(root, sys, first, last);
  }

  /// The entry point for command dispatching.
  command root;
};

/// Formats a useful human readable error message on the output stream.
/// @param app The application.
/// @param err An error generated by the application.
/// @param os The output stream to write to.
void render_error(const application& app, const caf::error& err,
                  std::ostream& os);

} // namespace vast::system
