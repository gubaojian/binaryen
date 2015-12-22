/*
 * Copyright 2015 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// Lowers 64-bit ints to pairs of 32-bit ints, plus some library routines
//
// This is useful for wasm2asm, as JS has no native 64-bit integer support.
//

#include <memory>

#include <wasm.h>
#include <pass.h>

namespace wasm {

cashew::IString GET_HIGH("getHigh");

struct LowerInt64 : public Pass {
  MixedArena* allocator;
  std::unique_ptr<NameManager> namer;

  void prepare(PassRunner* runner, Module *module) override {
    allocator = runner->allocator;
    namer = std::unique_ptr<NameManager>(new NameManager());
    namer->run(runner, module);
  }

  std::map<Expression*, Expression*> fixes; // fixed nodes, outputs of lowering, mapped to their high bits
  std::map<Name, Name> locals; // maps locals which were i64->i32 to their high bits

  void makeGetHigh() {
    auto ret = allocator->alloc<CallImport>();
    ret->target = GET_HIGH;
    ret->type = i32;
    return ret;
  }

  void fixCall(CallBase *call) {
    auto& operands = call->operands;
    for (size_t i = 0; i < operands.size(); i++) {
      auto fix = fixes.find(operands[i]);
      if (fix != fixes.end()) {
        operands.insert(operands.begin() + i + 1, *fix);
      }
    }
    if (curr->type == i64) {
      curr->type = i32;
      fixes[curr] = makeGetHigh(); // called function will setHigh
    }
  }

  void visitCall(Call *curr) override {
    fixCall(curr);
  }
  void visitCallImport(CallImport *curr) override {
    fixCall(curr);
  }
  void visitCallIndirect(CallIndirect *curr) override {
    fixCall(curr);
  }
  void visitGetLocal(GetLocal *curr) override {
    if (curr->type == i64) {
      if (locals.count(curr->name) == 0) {
        Name highName = namer->getUnique("high");
        locals[curr->name] = highName;
      };
      curr->type = i32;
      auto high = allocator->alloc<GetLocal>();
      high->name = locals[curr->name];
      high->type = i32;
      fixes[curr] = high;
    }
  }
  void visitSetLocal(SetLocal *curr) override {
    if (curr->type == i64) {
      Name highName;
      if (locals.count(curr->name) == 0) {
        highName = namer->getUnique("high");
        locals[curr->name] = highName;
      } else {
        highName = locals[curr->name];
      }
      curr->type = i32;
      auto high = allocator->alloc<GetLocal>();
      high->name = highName;
      high->type = i32;
      fixes[curr] = high;
      // Set the high bits
      auto set = allocator.alloc<SetLocal>();
      set->name = highName;
      set->value = fixes[curr->value];
      set->type = i32;
      assert(set->value);
      auto low = allocator->alloc<GetLocal>();
      low->name = curr->name;
      low->type = i32;
      auto ret = allocator.alloc<Block>();
      ret->list.push_back(curr);
      ret->list.push_back(set);
      ret->list.push_back(low); // so the block returns the low bits
      ret->type = i32;
      fixes[ret] = high;
      replaceCurrent(ret);
    }
  }
  void visitLoad(Load *curr) override {
  }
  void visitStore(Store *curr) override {
  }
  void visitConst(Const *curr) override {
  }
  void visitUnary(Unary *curr) override {
  }
  void visitBinary(Binary *curr) override {
  }
  void visitSelect(Select *curr) override {
  }
  void visitHost(Host *curr) override {
  }
  void visitNop(Nop *curr) override {
  }
  void visitUnreachable(Unreachable *curr) override {
  }

  void visitFunctionType(FunctionType *curr) override {
  }
  void visitImport(Import *curr) override {
  }
  void visitExport(Export *curr) override {
  }
  void visitFunction(Function *curr) override {
  }
  void visitTable(Table *curr) override {
  }
  void visitMemory(Memory *curr) override {
  }
  void visitModule(Module *curr) override {
  }
};

static RegisterPass<LowerInt64> registerPass("lower-i64", "lowers i64 into pairs of i32s");

} // namespace wasm
