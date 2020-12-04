//========================================================================================
// (C) (or copyright) 2020. Triad National Security, LLC. All rights reserved.
//
// This program was produced under U.S. Government contract 89233218CNA000001 for Los
// Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC
// for the U.S. Department of Energy/National Nuclear Security Administration. All rights
// in the program are reserved by Triad National Security, LLC, and the U.S. Department
// of Energy/National Nuclear Security Administration. The Government is granted for
// itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide
// license in this material to reproduce, prepare derivative works, distribute copies to
// the public, perform publicly and display publicly, and to permit others to do so.
//========================================================================================

#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "interface/metadata.hpp"
#include "interface/state_descriptor.hpp"

namespace parthenon {

// Helper class for ResolvePackages
struct DependencyTracker {
  std::unordered_set<std::string> provided_vars;
  std::unordered_set<std::string> depends_vars;
  std::unordered_map<std::string, int> overridable_vars;
  std::unordered_map<std::string, std::unordered_map<int, bool>> sparse_added;
  std::unordered_map<std::string, std::vector<Metadata>> overridable_meta;

  template <typename AdderPrivate, typename AdderProvides>
  void Sort(const std::string &package, const std::string &var, const Metadata &metadata,
            const AdderPrivate &add_private, const AdderProvides &add_provides) {
    auto dependency = metadata.Dependency();
    bool is_sparse = metadata.IsSet(Metadata::Sparse);
    int sparse_id = metadata.GetSparseId();
    if (dependency == Metadata::Private) {
      add_private(package, var, metadata);
    } else if (dependency == Metadata::Provides) {
      if (provided_vars.count(var) > 0) {
        PARTHENON_THROW("Variable " + var + " Provided by multiple packages");
      }
      provided_vars.insert(var);
      add_provides(package, var, metadata);
    } else if (dependency == Metadata::Requires) {
      depends_vars.insert(var);
    } else if (dependency == Metadata::Overridable) {
      if (overridable_meta.count(var) == 0) {
        overridable_meta[var] = {metadata};
        if (is_sparse) {
          sparse_added[var][sparse_id] = true;
        }
      }
      if (is_sparse) {
        if (!sparse_added[var][sparse_id]) {
          overridable_meta[var].push_back(metadata);
          sparse_added[var][sparse_id] = true;
        }
      }
      overridable_vars[var] += 1; // using value initalization of ints = 0
    } else {
      PARTHENON_THROW("Unknown dependency");
    }
  }

  template <typename Collection, typename AdderPrivate, typename AdderProvides>
  void SortCollection(const std::string &package, const Collection &c,
                      const AdderPrivate &add_private,
                      const AdderProvides &add_provides) {
    for (auto &pair : c) {
      std::string var = pair.first;
      auto &metadata = pair.second;
      Sort(package, var, metadata, add_private, add_provides);
    }
  }

  void CheckRequires() {
    for (auto &v : depends_vars) {
      if (provided_vars.count(v) == 0) {
        std::stringstream ss;
        ss << "Variable " << v
           << " registered as required, but not provided by any package!" << std::endl;
        PARTHENON_THROW(ss);
      }
    }
  }

  template <typename Adder>
  void CheckOverridable(Adder add_to_state) {
    std::unordered_set<std::string> cache;
    for (auto &pair : overridable_vars) {
      auto &var = pair.first;
      auto &count = pair.second;
      if (provided_vars.count(var) == 0) {
        if (count > 1) {
          std::stringstream ss;
          ss << "Variable " << var
             << " registered as overridable multiple times, but never provided."
             << " This results in undefined behaviour as to which package will provide"
             << " it." << std::endl;
          PARTHENON_DEBUG_WARN(ss);
        }
        auto &mvec = overridable_meta[var];
        for (auto &metadata : mvec) {
          add_to_state(var, metadata);
        }
      }
    }
  }

  bool Provided(const std::string &var) { return provided_vars.count(var) > 0; }
};

bool StateDescriptor::AddSwarmValue(const std::string &value_name,
                                    const std::string &swarm_name, const Metadata &m) {
  if (swarmMetadataMap_.count(swarm_name) == 0) {
    throw std::invalid_argument("Swarm " + swarm_name + " does not exist!");
  }
  if (swarmValueMetadataMap_[swarm_name].count(value_name) > 0) {
    throw std::invalid_argument("Swarm value " + value_name + " already exists!");
  }
  swarmValueMetadataMap_[swarm_name][value_name] = m;

  return true;
}

bool StateDescriptor::AddField(const std::string &field_name, const Metadata &m_in) {
  Metadata m = m_in; // horrible hack to force const correctness
  if (m.IsSet(Metadata::Sparse)) {
    auto miter = sparseMetadataMap_.find(field_name);
    if (miter != sparseMetadataMap_.end()) {
      auto &mvec = miter->second;
      PARTHENON_REQUIRE_THROWS(mvec[0].SparseEqual(m),
                               "All sparse variables with the same name must have the "
                               "same metadata flags and shape.");
      if (mvec[0].GetSparseId() == m.GetSparseId()) {
        return false;
      }
      mvec.push_back(m);
    } else {
      sparseMetadataMap_[field_name] = {m};
    }
  } else {
    const std::string &assoc = m.getAssociated();
    if (!assoc.length()) m.Associate(field_name);
    auto miter = metadataMap_.find(field_name);
    if (miter != metadataMap_.end()) { // this field has already been added
      Metadata &mprev = miter->second;
      return false;
    } else {
      m.Associate("");
      metadataMap_[field_name] = m;
    }
  }
  return true;
}

bool StateDescriptor::FlagsPresent(std::vector<MetadataFlag> const &flags,
                                   bool matchAny) {
  for (auto &pair : metadataMap_) {
    auto &metadata = pair.second;
    if (metadata.FlagsSet(flags, matchAny)) return true;
  }
  for (auto &pair : sparseMetadataMap_) {
    auto &sparsevec = pair.second;
    for (auto &metadata : sparsevec) {
      if (metadata.FlagsSet(flags, matchAny)) return true;
    }
  }
  return false;
}

void StateDescriptor::ValidateMetadata() {
  MetadataLoop([&](Metadata &m) {
    auto dependency = m.Dependency();
    if (dependency == Metadata::None) {
      m.Set(Metadata::Provides);
    }
  });
}

std::ostream &operator<<(std::ostream &os, const StateDescriptor &sd) {
  os << "# Package: " << sd.label() << "\n"
     << "# ---------------------------------------------------\n"
     << "# Variables:\n"
     << "# Name\tMetadata flags\n"
     << "# ---------------------------------------------------\n";
  for (auto &pair : sd.metadataMap_) {
    auto &var = pair.first;
    auto &metadata = pair.second;
    os << var << "\t" << metadata << "\n";
  }
  os << "# ---------------------------------------------------\n"
     << "# Sparse Variables:\n"
     << "# Name\tsparse id\tMetadata flags\n"
     << "# ---------------------------------------------------\n";
  for (auto &pair : sd.sparseMetadataMap_) {
    auto &var = pair.first;
    auto &mvec = pair.second;
    os << var << "\n";
    for (auto &metadata : mvec) {
      os << "    \t" << metadata.GetSparseId() << "\t" << metadata << "\n";
    }
  }
  os << "# ---------------------------------------------------\n"
     << "# Swarms:\n"
     << "# Swarm\tValue\tmetadata\n"
     << "# ---------------------------------------------------\n";
  for (auto &pair : sd.swarmValueMetadataMap_) {
    auto &swarm = pair.first;
    auto &svals = pair.second;
    os << swarm << "\n";
    for (auto &p2 : svals) {
      auto &val = p2.first;
      auto &metadata = p2.second;
      os << ("     \t" + val + "\t") << metadata << "\n";
    }
  }
  return os;
}

// Takes all packages and combines them into a single state descriptor
// containing all variables with conflicts resolved.  Note the new
// state descriptor DOES not have any of its function pointers set.
std::shared_ptr<StateDescriptor> ResolvePackages(Packages_t &packages) {
  using std::string;
  auto state = std::make_shared<StateDescriptor>("parthenon::resolved_state");

  // The workhorse data structure. Uses sets to cache which variables
  // are of what type.
  DependencyTracker var_tracker;
  DependencyTracker swarm_tracker;

  // TODO(JMM): The sparse stuff here will need to be revisited,
  // and likely become much simpler once we have dense on block.

  // Begin awful boilerplate
  // ======================================================================
  // Helper functions for adding vars
  auto add_private_var = [&](const string &package, const string &var,
                             const Metadata &metadata) {
    state->AddField(package + "::" + var, metadata);
  };
  auto add_provides_var = [&](const string &package, const string &var,
                              const Metadata &metadata) {
    state->AddField(var, metadata);
  };
  auto add_overridable_var = [&](const string &var, Metadata &metadata) {
    state->AddField(var, metadata);
  };
  // sparse
  auto add_private_sparse = [&](const string &package, const string &var,
                                const Metadata &metadata) {
    const std::string name = package + "::" + var;
    for (auto &m : packages[package]->AllSparseFields().at(var)) {
      state->AddField(name, m);
    }
  };
  auto add_provides_sparse = [&](const string &package, const string &var,
                                 const Metadata &metadata) {
    for (auto &m : packages[package]->AllSparseFields().at(var)) {
      state->AddField(var, m);
    }
  };
  // swarm
  auto AddSwarm = [&](StateDescriptor *package, const string &swarm,
                      const string &swarm_name, const Metadata &metadata) {
    state->AddSwarm(swarm_name, metadata);
    for (auto &p : package->AllSwarmValues(swarm)) {
      auto &val_name = p.first;
      auto &val_meta = p.second;
      state->AddSwarmValue(val_name, swarm_name, val_meta);
    }
  };
  auto add_private_swarm = [&](const string &package, const string &var,
                               const Metadata &metadata) {
    AddSwarm(packages[package].get(), var, package + "::" + var, metadata);
  };
  auto add_provides_swarm = [&](const string &package, const string &var,
                                const Metadata &metadata) {
    AddSwarm(packages[package].get(), var, var, metadata);
  };
  auto add_overridable_swarm = [&](const string &swarm, const Metadata &metadata) {
    state->AddSwarm(swarm, metadata);
    for (auto &pair : packages) {
      auto &package = pair.second;
      if (package->SwarmPresent(swarm)) {
        for (auto &pair : package->AllSwarmValues(swarm)) {
          state->AddSwarmValue(pair.first, swarm, pair.second);
        }
        return;
      }
    }
  };
  // ======================================================================
  // end awful boilerplate
  // ======================================================================

  // Add private/provides variables. Check for conflicts among those.
  // Track dependent and overridable variables.
  for (auto &pair : packages) {
    auto &package = pair.second;
    package->ValidateMetadata(); // set unset flags
    // sort
    var_tracker.SortCollection(package->label(), package->AllFields(), add_private_var,
                               add_provides_var);
    for (auto &p2 : package->AllSparseFields()) { // sparse
      auto &var = p2.first;
      auto &mvec = p2.second;
      for (auto &metadata : mvec) {
        var_tracker.Sort(package->label(), var, metadata, add_private_sparse,
                         add_provides_sparse);
      }
    }
    swarm_tracker.SortCollection(package->label(), package->AllSwarms(),
                                 add_private_swarm, add_provides_swarm);
  }

  // check that dependent variables are provided somewhere
  var_tracker.CheckRequires();
  swarm_tracker.CheckRequires();

  // Treat overridable vars:
  // If a var is overridable and provided, do nothing.
  // If a var is overridable and unique, add it to the state.
  // If a var is overridable and not unique, add one to the state
  // and optionally throw a warning.
  var_tracker.CheckOverridable(add_overridable_var);
  swarm_tracker.CheckOverridable(add_overridable_swarm);

  return state;
}

} // namespace parthenon
