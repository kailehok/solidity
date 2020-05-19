/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * Class that contains contextual information during IR generation.
 */

#include <libsolidity/codegen/ir/IRGenerationContext.h>

#include <libsolidity/codegen/YulUtilFunctions.h>
#include <libsolidity/codegen/ABIFunctions.h>
#include <libsolidity/codegen/CompilerUtils.h>
#include <libsolidity/ast/AST.h>
#include <libsolidity/ast/TypeProvider.h>

#include <libsolutil/Whiskers.h>
#include <libsolutil/StringUtils.h>

#include <boost/range/adaptor/map.hpp>

using namespace std;
using namespace solidity;
using namespace solidity::util;
using namespace solidity::frontend;

string IRGenerationContext::enqueueFunctionForCodeGeneration(FunctionDefinition const& _function)
{
	string name = IRNames::function(_function);

	if (!m_functions.contains(name))
		m_functionGenerationQueue.insert(&_function);

	return name;
}

FunctionDefinition const* IRGenerationContext::dequeueFunctionForCodeGeneration()
{
	solAssert(!m_functionGenerationQueue.empty(), "");

	FunctionDefinition const* result = *m_functionGenerationQueue.begin();
	m_functionGenerationQueue.erase(m_functionGenerationQueue.begin());
	return result;
}

ContractDefinition const& IRGenerationContext::mostDerivedContract() const
{
	solAssert(m_mostDerivedContract, "Most derived contract requested but not set.");
	return *m_mostDerivedContract;
}

IRVariable const& IRGenerationContext::addLocalVariable(VariableDeclaration const& _varDecl)
{
	auto const& [it, didInsert] = m_localVariables.emplace(
		std::make_pair(&_varDecl, IRVariable{_varDecl})
	);
	solAssert(didInsert, "Local variable added multiple times.");
	return it->second;
}

IRVariable const& IRGenerationContext::localVariable(VariableDeclaration const& _varDecl)
{
	solAssert(
		m_localVariables.count(&_varDecl),
		"Unknown variable: " + _varDecl.name()
	);
	return m_localVariables.at(&_varDecl);
}

void IRGenerationContext::registerImmutableVariable(VariableDeclaration const& _variable)
{
	solAssert(_variable.immutable(), "Attempted to register a non-immutable variable as immutable.");
	solUnimplementedAssert(
		_variable.annotation().type->isValueType(),
		"Only immutable variables of value type are supported."
	);
	solAssert(m_reservedMemory.has_value(), "Reserved memory has already been reset.");
	m_immutableVariables[&_variable] = CompilerUtils::generalPurposeMemoryStart + *m_reservedMemory;
	solAssert(_variable.annotation().type->memoryHeadSize() == 32, "Memory writes might overlap.");
	*m_reservedMemory += _variable.annotation().type->memoryHeadSize();
}

size_t IRGenerationContext::immutableMemoryOffset(VariableDeclaration const& _variable) const
{
	solAssert(
		m_immutableVariables.count(&_variable),
		"Unknown immutable variable: " + _variable.name()
	);
	return m_immutableVariables.at(&_variable);
}

size_t IRGenerationContext::reservedMemory()
{
	solAssert(m_reservedMemory.has_value(), "Reserved memory was used before.");
	size_t reservedMemory = *m_reservedMemory;
	m_reservedMemory = std::nullopt;
	return reservedMemory;
}

void IRGenerationContext::addStateVariable(
	VariableDeclaration const& _declaration,
	u256 _storageOffset,
	unsigned _byteOffset
)
{
	m_stateVariables[&_declaration] = make_pair(move(_storageOffset), _byteOffset);
}

string IRGenerationContext::newYulVariable()
{
	return "_" + to_string(++m_varCounter);
}

void IRGenerationContext::initializeInternalDispatch(InternalDispatchMap _internalDispatch)
{
	solAssert(internalDispatchClean(), "");

	for (set<FunctionDefinition const*> const& functions: _internalDispatch | boost::adaptors::map_values)
		for (auto function: functions)
			enqueueFunctionForCodeGeneration(*function);

	m_internalDispatchMap = move(_internalDispatch);
}

InternalDispatchMap IRGenerationContext::consumeInternalDispatchMap()
{
	m_directInternalFunctionCalls.clear();

	InternalDispatchMap internalDispatch = move(m_internalDispatchMap);
	m_internalDispatchMap.clear();
	return internalDispatch;
}

void IRGenerationContext::internalFunctionCalledDirectly(Expression const& _expression)
{
	solAssert(m_directInternalFunctionCalls.count(&_expression) == 0, "");

	m_directInternalFunctionCalls.insert(&_expression);
}

void IRGenerationContext::internalFunctionAccessed(Expression const& _expression)
{
	solAssert(IRHelpers::referencedFunctionDeclaration(_expression), "");

	if (m_directInternalFunctionCalls.count(&_expression) == 0)
	{
		FunctionDefinition const* function = IRHelpers::referencedFunctionDeclaration(_expression);
		FunctionType const* functionType = TypeProvider::function(*function, FunctionType::Kind::Internal);
		solAssert(functionType, "");

		YulArity arity = YulArity::fromType(*functionType);
		m_internalDispatchMap.try_emplace(arity);
		m_internalDispatchMap[arity].insert(function);
		enqueueFunctionForCodeGeneration(*function);
	}
}

void IRGenerationContext::internalFunctionCalledViaPointer(YulArity const& _arity)
{
	m_internalDispatchMap.try_emplace(_arity);
}

InternalDispatchMap IRGenerationContext::generateInternalDispatchFunctions()
{
	solAssert(
		functionGenerationQueueEmpty(),
		"At this point all the enqueued functions should have been generated. "
		"Otherwise the dispatch may be incomplete."
	);

	InternalDispatchMap internalDispatchMap = consumeInternalDispatchMap();
	for (YulArity const& arity: internalDispatchMap | boost::adaptors::map_keys)
	{
		string funName = IRNames::internalDispatch(arity);
		m_functions.createFunction(funName, [&]() {
			Whiskers templ(R"(
				function <functionName>(fun <comma> <in>) <arrow> <out> {
					switch fun
					<#cases>
					case <funID>
					{
						<out> <assignment_op> <name>(<in>)
					}
					</cases>
					default { invalid() }
				}
			)");
			templ("functionName", funName);
			templ("comma", arity.in > 0 ? "," : "");
			templ("in", suffixedVariableNameList("in_", 0, arity.in));
			templ("arrow", arity.out > 0 ? "->" : "");
			templ("assignment_op", arity.out > 0 ? ":=" : "");
			templ("out", suffixedVariableNameList("out_", 0, arity.out));

			vector<map<string, string>> cases;
			for (FunctionDefinition const* function: internalDispatchMap.at(arity))
			{
				solAssert(function, "");
				solAssert(
					YulArity::fromType(*TypeProvider::function(*function, FunctionType::Kind::Internal)) == arity,
					"A single dispatch function can only handle functions of one arity"
				);
				solAssert(!function->isConstructor(), "");
				// 0 is reserved for uninitialized function pointers
				solAssert(function->id() != 0, "Unexpected function ID: 0");

				cases.emplace_back(map<string, string>{
					{"funID", to_string(function->id())},
					{"name", IRNames::function(*function)}
				});

				enqueueFunctionForCodeGeneration(*function);
			}

			templ("cases", move(cases));
			return templ.render();
		});
	}

	solAssert(internalDispatchClean(), "");
	solAssert(
		functionGenerationQueueEmpty(),
		"Internal dispatch generation must not add new functions to generation queue because they won't be proeessed."
	);

	return internalDispatchMap;
}

YulUtilFunctions IRGenerationContext::utils()
{
	return YulUtilFunctions(m_evmVersion, m_revertStrings, m_functions);
}

ABIFunctions IRGenerationContext::abiFunctions()
{
	return ABIFunctions(m_evmVersion, m_revertStrings, m_functions);
}

std::string IRGenerationContext::revertReasonIfDebug(std::string const& _message)
{
	return YulUtilFunctions::revertReasonIfDebug(m_revertStrings, _message);
}
