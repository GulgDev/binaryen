#include "ir/module-utils.h"
#include "ir/names.h"
#include "support/colors.h"
#include "support/file.h"
#include "wasm-builder.h"
#include "wasm-io.h"
#include "wasm-validator.h"
#include "wasm.h"

namespace wasm {
  struct WasmVisitor
    : public WalkerPass<
        PostWalker<WasmVisitor, UnifiedExpressionVisitor<WasmVisitor>>> {
    bool isFunctionParallel() override { return true; }

    std::unique_ptr<Pass> create() override {
      return std::make_unique<WasmVisitor>();
    }

    void visitExpression(Expression* curr) {
#define DELEGATE_ID curr->_id

#define DELEGATE_START(id) [[maybe_unused]] auto* cast = curr->cast<id>();

#define DELEGATE_GET_FIELD(id, field) cast->field

#define DELEGATE_FIELD_TYPE(id, field)
#define DELEGATE_FIELD_HEAPTYPE(id, field)
#define DELEGATE_FIELD_CHILD(id, field)
#define DELEGATE_FIELD_OPTIONAL_CHILD(id, field)
#define DELEGATE_FIELD_INT(id, field)
#define DELEGATE_FIELD_LITERAL(id, field)
#define DELEGATE_FIELD_NAME(id, field)
#define DELEGATE_FIELD_SCOPE_NAME_DEF(id, field)
#define DELEGATE_FIELD_SCOPE_NAME_USE(id, field)
#define DELEGATE_FIELD_ADDRESS(id, field)

#define DELEGATE_FIELD_NAME_KIND(id, field, kind)                              \
  if (cast->field.is()) {                                                      \
    mapName(kind, cast->field);                                                \
  }

#include "wasm-delegations-fields.def"
    }

    // Aside from expressions, we have a few other things we need to update at
    // the module scope.
    void mapModuleFields(Module& wasm) {
      for (auto& curr : wasm.exports) {
        // skip type exports
        if (auto* name = curr->getInternalName()) {
          mapName(ModuleItemKind(curr->kind), *name);
        }
      }
      for (auto& curr : wasm.elementSegments) {
        mapName(ModuleItemKind::Table, curr->table);
      }
      for (auto& curr : wasm.dataSegments) {
        mapName(ModuleItemKind::Memory, curr->memory);
      }

      mapName(ModuleItemKind::Function, wasm.start);
    }

  private:
    Name resolveName(NameUpdates& updates, Name newName, Name oldName) {
      // Iteratively lookup the updated name.
      std::set<Name> visited;
      auto name = newName;
      while (1) {
        auto iter = updates.find(name);
        if (iter == updates.end()) {
          return name;
        }
        if (visited.count(name)) {
          // This is a loop of imports, which means we cannot resolve a useful
          // name. Report an error.
          Fatal() << "wasm-merge: infinite loop of imports on " << oldName;
        }
        visited.insert(name);
        name = iter->second;
      }
    }

    void mapName(ModuleItemKind kind, Name& name) {
      auto iter = kindNameUpdates.find(kind);
      if (iter == kindNameUpdates.end()) {
        return;
      }
      auto& nameUpdates = iter->second;
      auto iter2 = nameUpdates.find(name);
      if (iter2 != nameUpdates.end()) {
        name = resolveName(nameUpdates, iter2->second, name);
      }
    }
  };
} // namespace wasm