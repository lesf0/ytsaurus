#pragma once
#include <yql/essentials/minikql/computation/mkql_computation_node.h>

namespace NKikimr {
namespace NMiniKQL {

IComputationNode* WrapWideCondense1(TCallable& callable, const TComputationNodeFactoryContext& ctx);

}
}

