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

#include "vast/system/spawn_metastore.hpp"

#include <caf/actor.hpp>
#include <caf/actor_cast.hpp>
#include <caf/expected.hpp>
#include <caf/send.hpp>

#include "vast/detail/unbox_var.hpp"
#include "vast/system/consensus.hpp"
#include "vast/system/node.hpp"
#include "vast/system/replicated_store.hpp"
#include "vast/system/spawn_arguments.hpp"

namespace vast::system {

maybe_actor spawn_metastore(caf::local_actor* self, spawn_arguments& args) {
  if (!args.empty())
    return unexpected_arguments(args);
  auto id = get_or(args.options, "global.id", raft::server_id{0});
  // Bring up the consensus module.
  auto consensus = self->spawn(raft::consensus, args.dir / "consensus");
  self->monitor(consensus);
  if (id != 0)
    caf::anon_send(consensus, id_atom::value, id);
  anon_send(consensus, run_atom::value);
  // Spawn the store on top.
  auto s = self->spawn(replicated_store<std::string, data>, consensus);
  s->attach_functor(
    [=](const error&) {
      anon_send_exit(consensus, caf::exit_reason::user_shutdown);
    }
  );
  return caf::actor_cast<caf::actor>(s);
}

} // namespace vast::system
