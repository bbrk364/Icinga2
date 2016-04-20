/******************************************************************************
 * Icinga 2                                                                   *
 * Copyright (C) 2012-2016 Icinga Development Team (https://www.icinga.org/)  *
 *                                                                            *
 * This program is free software; you can redistribute it and/or              *
 * modify it under the terms of the GNU General Public License                *
 * as published by the Free Software Foundation; either version 2             *
 * of the License, or (at your option) any later version.                     *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program; if not, write to the Free Software Foundation     *
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ******************************************************************************/

#include "config/expression.hpp"
#include "config/configitem.hpp"
#include "config/configcompiler.hpp"
#include "config/vmops.hpp"
#include "config/jitops.hpp"
#include "base/array.hpp"
#include "base/json.hpp"
#include "base/object.hpp"
#include "base/logger.hpp"
#include "base/exception.hpp"
#include "base/scriptglobal.hpp"
#include "base/loader.hpp"
#include <boost/foreach.hpp>
#include <boost/exception_ptr.hpp>
#include <boost/exception/errinfo_nested_exception.hpp>

using namespace icinga;

boost::signals2::signal<void (ScriptFrame&, ScriptError *ex, const DebugInfo&)> Expression::OnBreakpoint;
boost::thread_specific_ptr<bool> l_InBreakpointHandler;

Expression::~Expression(void)
{ }

void Expression::ScriptBreakpoint(ScriptFrame& frame, ScriptError *ex, const DebugInfo& di)
{
	bool *inHandler = l_InBreakpointHandler.get();
	if (!inHandler || !*inHandler) {
		inHandler = new bool(true);
		l_InBreakpointHandler.reset(inHandler);
		OnBreakpoint(frame, ex, di);
		*inHandler = false;
	}
}

ExpressionResult Expression::Evaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	try {
#ifdef I2_DEBUG
/*		std::ostringstream msgbuf;
		ShowCodeLocation(msgbuf, GetDebugInfo(), false);
		Log(LogDebug, "Expression")
			<< "Executing:\n" << msgbuf.str();*/
#endif /* I2_DEBUG */

		frame.IncreaseStackDepth();
		ExpressionResult result = DoEvaluate(frame, dhint);
		frame.DecreaseStackDepth();
		return result;
	} catch (ScriptError& ex) {
		frame.DecreaseStackDepth();

		ScriptBreakpoint(frame, &ex, GetDebugInfo());
		throw;
	} catch (const std::exception& ex) {
		frame.DecreaseStackDepth();

		BOOST_THROW_EXCEPTION(ScriptError("Error while evaluating expression: " + String(ex.what()), GetDebugInfo())
		    << boost::errinfo_nested_exception(boost::current_exception()));
	}

	frame.DecreaseStackDepth();
}

bool Expression::GetReference(ScriptFrame& frame, bool init_dict, Value *parent, String *index, DebugHint **dhint) const
{
	return false;
}

const DebugInfo& Expression::GetDebugInfo(void) const
{
	static DebugInfo debugInfo;
	return debugInfo;
}

Expression *icinga::MakeIndexer(ScopeSpecifier scopeSpec, const String& index)
{
	Expression *scope = new GetScopeExpression(scopeSpec);
	return new IndexerExpression(scope, MakeLiteral(index));
}

void DictExpression::MakeInline(void)
{
	m_Inline = true;
}

LiteralExpression::LiteralExpression(const Value& value)
	: m_Value(value)
{ }

ExpressionResult LiteralExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return m_Value;
}

bool LiteralExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	EmitJitNewValue(dtor, evaluate, m_Value, res);

	delete this;

	return true;
}

const DebugInfo& DebuggableExpression::GetDebugInfo(void) const
{
	return m_DebugInfo;
}

ExpressionResult VariableExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return VMOps::Variable(frame, m_Variable, m_DebugInfo);
}

bool VariableExpression::GetReference(ScriptFrame& frame, bool init_dict, Value *parent, String *index, DebugHint **dhint) const
{
	*index = m_Variable;

	if (frame.Locals && frame.Locals->Contains(m_Variable)) {
		*parent = frame.Locals;

		if (dhint)
			*dhint = NULL;

		return true;
	} else if (frame.Self.IsObject()) {
		const Object::Ptr& self = frame.Self;

		if (frame.Locals != self && self->HasOwnField(m_Variable)) {
			*parent = frame.Self;

			if (dhint && *dhint)
				*dhint = new DebugHint((*dhint)->GetChild(m_Variable));

			return true;
		}
	}

	if (ScriptGlobal::Exists(m_Variable)) {
		*parent = ScriptGlobal::GetGlobals();

		if (dhint)
			*dhint = NULL;
	} else
		*parent = frame.Self;

	return true;
}

ExpressionResult NegateExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return ~(long)m_Operand->Evaluate(frame).Result;
}

bool NegateExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	EmitJitExpression(dtor, evaluate, m_Operand, frame, dhint, res);
	m_Operand = NULL;

	EmitJitValueNegate(evaluate, res);

	delete this;

	return true;
}

ExpressionResult LogicalNegateExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return !m_Operand->Evaluate(frame).Result.ToBool();
}

bool LogicalNegateExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	EmitJitExpression(dtor, evaluate, m_Operand, frame, dhint, res);
	m_Operand = NULL;

	EmitJitValueLogicalNegate(evaluate, res);

	delete this;

	return true;
}

ExpressionResult AddExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return m_Operand1->Evaluate(frame).Result + m_Operand2->Evaluate(frame).Result;
}

bool AddExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	asmjit::X86Mem op1 = evaluate.newStack(sizeof(Value), 8);

	asmjit::X86GpVar pop1 = evaluate.newIntPtr();
	evaluate.lea(pop1, op1);

	EmitJitExpression(dtor, evaluate, m_Operand1, frame, dhint, pop1);
	m_Operand1 = NULL;

	EmitJitExpression(dtor, evaluate, m_Operand2, frame, dhint, res);
	m_Operand2 = NULL;

	EmitJitValueAdd(evaluate, pop1, res);
	EmitJitDtorValue(evaluate, pop1);

	delete this;

	return true;
}

ExpressionResult SubtractExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return m_Operand1->Evaluate(frame).Result - m_Operand2->Evaluate(frame).Result;
}

bool SubtractExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	asmjit::X86Mem op1 = evaluate.newStack(sizeof(Value), 8);

	asmjit::X86GpVar pop1 = evaluate.newIntPtr();
	evaluate.lea(pop1, op1);

	EmitJitExpression(dtor, evaluate, m_Operand1, frame, dhint, pop1);
	m_Operand1 = NULL;

	EmitJitExpression(dtor, evaluate, m_Operand2, frame, dhint, res);
	m_Operand2 = NULL;

	EmitJitValueSubtract(evaluate, pop1, res);
	EmitJitDtorValue(evaluate, pop1);

	delete this;

	return true;
}

ExpressionResult MultiplyExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return m_Operand1->Evaluate(frame).Result * m_Operand2->Evaluate(frame).Result;
}


bool MultiplyExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	asmjit::X86Mem op1 = evaluate.newStack(sizeof(Value), 8);

	asmjit::X86GpVar pop1 = evaluate.newIntPtr();
	evaluate.lea(pop1, op1);

	EmitJitExpression(dtor, evaluate, m_Operand1, frame, dhint, pop1);
	m_Operand1 = NULL;

	EmitJitExpression(dtor, evaluate, m_Operand2, frame, dhint, res);
	m_Operand2 = NULL;

	EmitJitValueMultiply(evaluate, pop1, res);
	EmitJitDtorValue(evaluate, pop1);

	delete this;

	return true;
}

ExpressionResult DivideExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return m_Operand1->Evaluate(frame).Result / m_Operand2->Evaluate(frame).Result;
}


bool DivideExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	asmjit::X86Mem op1 = evaluate.newStack(sizeof(Value), 8);

	asmjit::X86GpVar pop1 = evaluate.newIntPtr();
	evaluate.lea(pop1, op1);

	EmitJitExpression(dtor, evaluate, m_Operand1, frame, dhint, pop1);
	m_Operand1 = NULL;

	EmitJitExpression(dtor, evaluate, m_Operand2, frame, dhint, res);
	m_Operand2 = NULL;

	EmitJitValueDivide(evaluate, pop1, res);
	EmitJitDtorValue(evaluate, pop1);

	delete this;

	return true;
}

ExpressionResult ModuloExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return m_Operand1->Evaluate(frame).Result % m_Operand2->Evaluate(frame).Result;
}


bool ModuloExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	asmjit::X86Mem op1 = evaluate.newStack(sizeof(Value), 8);

	asmjit::X86GpVar pop1 = evaluate.newIntPtr();
	evaluate.lea(pop1, op1);

	EmitJitExpression(dtor, evaluate, m_Operand1, frame, dhint, pop1);
	m_Operand1 = NULL;

	EmitJitExpression(dtor, evaluate, m_Operand2, frame, dhint, res);
	m_Operand2 = NULL;

	EmitJitValueModulo(evaluate, pop1, res);
	EmitJitDtorValue(evaluate, pop1);

	delete this;

	return true;
}

ExpressionResult XorExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return m_Operand1->Evaluate(frame).Result ^ m_Operand2->Evaluate(frame).Result;
}


bool XorExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	asmjit::X86Mem op1 = evaluate.newStack(sizeof(Value), 8);

	asmjit::X86GpVar pop1 = evaluate.newIntPtr();
	evaluate.lea(pop1, op1);

	EmitJitExpression(dtor, evaluate, m_Operand1, frame, dhint, pop1);
	m_Operand1 = NULL;

	EmitJitExpression(dtor, evaluate, m_Operand2, frame, dhint, res);
	m_Operand2 = NULL;

	EmitJitValueXor(evaluate, pop1, res);
	EmitJitDtorValue(evaluate, pop1);

	delete this;

	return true;
}

ExpressionResult BinaryAndExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return m_Operand1->Evaluate(frame).Result & m_Operand2->Evaluate(frame).Result;
}

bool BinaryAndExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	asmjit::X86Mem op1 = evaluate.newStack(sizeof(Value), 8);

	asmjit::X86GpVar pop1 = evaluate.newIntPtr();
	evaluate.lea(pop1, op1);

	EmitJitExpression(dtor, evaluate, m_Operand1, frame, dhint, pop1);
	m_Operand1 = NULL;

	EmitJitExpression(dtor, evaluate, m_Operand2, frame, dhint, res);
	m_Operand2 = NULL;

	EmitJitValueBinaryAnd(evaluate, pop1, res);
	EmitJitDtorValue(evaluate, pop1);

	delete this;

	return true;
}

ExpressionResult BinaryOrExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return m_Operand1->Evaluate(frame).Result | m_Operand2->Evaluate(frame).Result;
}


bool BinaryOrExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	asmjit::X86Mem op1 = evaluate.newStack(sizeof(Value), 8);

	asmjit::X86GpVar pop1 = evaluate.newIntPtr();
	evaluate.lea(pop1, op1);

	EmitJitExpression(dtor, evaluate, m_Operand1, frame, dhint, pop1);
	m_Operand1 = NULL;

	EmitJitExpression(dtor, evaluate, m_Operand2, frame, dhint, res);
	m_Operand2 = NULL;

	EmitJitValueBinaryOr(evaluate, pop1, res);
	EmitJitDtorValue(evaluate, pop1);

	delete this;

	return true;
}

ExpressionResult ShiftLeftExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return m_Operand1->Evaluate(frame).Result << m_Operand2->Evaluate(frame).Result;
}


bool ShiftLeftExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	asmjit::X86Mem op1 = evaluate.newStack(sizeof(Value), 8);

	asmjit::X86GpVar pop1 = evaluate.newIntPtr();
	evaluate.lea(pop1, op1);

	EmitJitExpression(dtor, evaluate, m_Operand1, frame, dhint, pop1);
	m_Operand1 = NULL;

	EmitJitExpression(dtor, evaluate, m_Operand2, frame, dhint, res);
	m_Operand2 = NULL;

	EmitJitValueShiftLeft(evaluate, pop1, res);
	EmitJitDtorValue(evaluate, pop1);

	delete this;

	return true;
}

ExpressionResult ShiftRightExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return m_Operand1->Evaluate(frame).Result >> m_Operand2->Evaluate(frame).Result;
}


bool ShiftRightExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	asmjit::X86Mem op1 = evaluate.newStack(sizeof(Value), 8);

	asmjit::X86GpVar pop1 = evaluate.newIntPtr();
	evaluate.lea(pop1, op1);

	EmitJitExpression(dtor, evaluate, m_Operand1, frame, dhint, pop1);
	m_Operand1 = NULL;

	EmitJitExpression(dtor, evaluate, m_Operand2, frame, dhint, res);
	m_Operand2 = NULL;

	EmitJitValueShiftRight(evaluate, pop1, res);
	EmitJitDtorValue(evaluate, pop1);

	delete this;

	return true;
}

ExpressionResult EqualExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return m_Operand1->Evaluate(frame).Result == m_Operand2->Evaluate(frame).Result;
}


bool EqualExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	asmjit::X86Mem op1 = evaluate.newStack(sizeof(Value), 8);

	asmjit::X86GpVar pop1 = evaluate.newIntPtr();
	evaluate.lea(pop1, op1);

	EmitJitExpression(dtor, evaluate, m_Operand1, frame, dhint, pop1);
	m_Operand1 = NULL;

	EmitJitExpression(dtor, evaluate, m_Operand2, frame, dhint, res);
	m_Operand2 = NULL;

	EmitJitValueEqual(evaluate, pop1, res);
	EmitJitDtorValue(evaluate, pop1);

	delete this;

	return true;
}

ExpressionResult NotEqualExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return m_Operand1->Evaluate(frame).Result != m_Operand2->Evaluate(frame).Result;
}


bool NotEqualExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	asmjit::X86Mem op1 = evaluate.newStack(sizeof(Value), 8);

	asmjit::X86GpVar pop1 = evaluate.newIntPtr();
	evaluate.lea(pop1, op1);

	EmitJitExpression(dtor, evaluate, m_Operand1, frame, dhint, pop1);
	m_Operand1 = NULL;

	EmitJitExpression(dtor, evaluate, m_Operand2, frame, dhint, res);
	m_Operand2 = NULL;

	EmitJitValueNotEqual(evaluate, pop1, res);
	EmitJitDtorValue(evaluate, pop1);

	delete this;

	return true;
}

ExpressionResult LessThanExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return m_Operand1->Evaluate(frame).Result < m_Operand2->Evaluate(frame).Result;
}


bool LessThanExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	asmjit::X86Mem op1 = evaluate.newStack(sizeof(Value), 8);

	asmjit::X86GpVar pop1 = evaluate.newIntPtr();
	evaluate.lea(pop1, op1);

	EmitJitExpression(dtor, evaluate, m_Operand1, frame, dhint, pop1);
	m_Operand1 = NULL;

	EmitJitExpression(dtor, evaluate, m_Operand2, frame, dhint, res);
	m_Operand2 = NULL;

	EmitJitValueLessThan(evaluate, pop1, res);
	EmitJitDtorValue(evaluate, pop1);

	delete this;

	return true;
}

ExpressionResult GreaterThanExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return m_Operand1->Evaluate(frame).Result > m_Operand2->Evaluate(frame).Result;
}


bool GreaterThanExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	asmjit::X86Mem op1 = evaluate.newStack(sizeof(Value), 8);

	asmjit::X86GpVar pop1 = evaluate.newIntPtr();
	evaluate.lea(pop1, op1);

	EmitJitExpression(dtor, evaluate, m_Operand1, frame, dhint, pop1);
	m_Operand1 = NULL;

	EmitJitExpression(dtor, evaluate, m_Operand2, frame, dhint, res);
	m_Operand2 = NULL;

	EmitJitValueGreaterThan(evaluate, pop1, res);
	EmitJitDtorValue(evaluate, pop1);

	delete this;

	return true;
}

ExpressionResult LessThanOrEqualExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return m_Operand1->Evaluate(frame).Result <= m_Operand2->Evaluate(frame).Result;
}


bool LessThanOrEqualExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	asmjit::X86Mem op1 = evaluate.newStack(sizeof(Value), 8);

	asmjit::X86GpVar pop1 = evaluate.newIntPtr();
	evaluate.lea(pop1, op1);

	EmitJitExpression(dtor, evaluate, m_Operand1, frame, dhint, pop1);
	m_Operand1 = NULL;

	EmitJitExpression(dtor, evaluate, m_Operand2, frame, dhint, res);
	m_Operand2 = NULL;

	EmitJitValueLessThanOrEqual(evaluate, pop1, res);
	EmitJitDtorValue(evaluate, pop1);

	delete this;

	return true;
}

ExpressionResult GreaterThanOrEqualExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return m_Operand1->Evaluate(frame).Result >= m_Operand2->Evaluate(frame).Result;
}


bool GreaterThanOrEqualExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	asmjit::X86Mem op1 = evaluate.newStack(sizeof(Value), 8);

	asmjit::X86GpVar pop1 = evaluate.newIntPtr();
	evaluate.lea(pop1, op1);

	EmitJitExpression(dtor, evaluate, m_Operand1, frame, dhint, pop1);
	m_Operand1 = NULL;

	EmitJitExpression(dtor, evaluate, m_Operand2, frame, dhint, res);
	m_Operand2 = NULL;

	EmitJitValueGreaterThanOrEqual(evaluate, pop1, res);
	EmitJitDtorValue(evaluate, pop1);

	delete this;

	return true;
}

ExpressionResult InExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	ExpressionResult operand2 = m_Operand2->Evaluate(frame);

	if (operand2.Result.IsEmpty())
		return false;
	else if (!operand2.Result.IsObjectType<Array>())
		BOOST_THROW_EXCEPTION(ScriptError("Invalid right side argument for 'in' operator: " + JsonEncode(operand2.Result), m_DebugInfo));

	Array::Ptr arr = operand2.Result;
	return arr->Contains(m_Operand1->Evaluate(frame).Result);
}

bool InExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	JIT_REPLACE_EXPRESSION(m_Operand1);
	JIT_REPLACE_EXPRESSION(m_Operand2);

	return false;
}

ExpressionResult NotInExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	ExpressionResult operand2 = m_Operand2->Evaluate(frame);

	if (operand2.Result.IsEmpty())
		return true;
	else if (!operand2.Result.IsObjectType<Array>())
		BOOST_THROW_EXCEPTION(ScriptError("Invalid right side argument for 'in' operator: " + JsonEncode(operand2.Result), m_DebugInfo));

	Array::Ptr arr = operand2.Result;
	return !arr->Contains(m_Operand1->Evaluate(frame));
}

bool NotInExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	JIT_REPLACE_EXPRESSION(m_Operand1);
	JIT_REPLACE_EXPRESSION(m_Operand2);

	return false;
}

ExpressionResult LogicalAndExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	ExpressionResult operand1 = m_Operand1->Evaluate(frame);

	if (!operand1.Result.ToBool())
		return operand1;
	else
		return m_Operand2->Evaluate(frame).Result;
}

bool LogicalAndExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	EmitJitExpression(dtor, evaluate, m_Operand1, frame, dhint, res);

	asmjit::X86GpVar eres = EmitJitValueIsTrue(evaluate, res);

	asmjit::Label after_if = evaluate.newLabel();

	evaluate.cmp(eres, 0);
	evaluate.jz(after_if);

	EmitJitDtorValue(evaluate, res);
	EmitJitExpression(dtor, evaluate, m_Operand2, frame, dhint, res);

	evaluate.bind(after_if);

	return true;
}

ExpressionResult LogicalOrExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	ExpressionResult operand1 = m_Operand1->Evaluate(frame);

	if (operand1.Result.ToBool())
		return operand1;
	else
		return m_Operand2->Evaluate(frame).Result;
}

bool LogicalOrExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	EmitJitExpression(dtor, evaluate, m_Operand1, frame, dhint, res);

	asmjit::X86GpVar eres = EmitJitValueIsTrue(evaluate, res);

	asmjit::Label after_if = evaluate.newLabel();

	evaluate.cmp(eres, 0);
	evaluate.jnz(after_if);

	EmitJitDtorValue(evaluate, res);
	EmitJitExpression(dtor, evaluate, m_Operand2, frame, dhint, res);

	evaluate.bind(after_if);

	return true;
}

ExpressionResult FunctionCallExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	Value self, vfunc;
	String index;

	if (m_FName->GetReference(frame, false, &self, &index))
		vfunc = VMOps::GetField(self, index, frame.Sandboxed, m_DebugInfo);
	else
		vfunc = m_FName->Evaluate(frame).Result;

	if (vfunc.IsObjectType<Type>()) {
		std::vector<Value> arguments;
		BOOST_FOREACH(Expression *arg, m_Args) {
			arguments.push_back(arg->Evaluate(frame).Result);
		}

		return VMOps::ConstructorCall(vfunc, arguments, m_DebugInfo);
	}

	if (!vfunc.IsObjectType<Function>())
		BOOST_THROW_EXCEPTION(ScriptError("Argument is not a callable object.", m_DebugInfo));

	Function::Ptr func = vfunc;

	if (!func->IsSideEffectFree() && frame.Sandboxed)
		BOOST_THROW_EXCEPTION(ScriptError("Function is not marked as safe for sandbox mode.", m_DebugInfo));

	std::vector<Value> arguments;
	BOOST_FOREACH(Expression *arg, m_Args) {
		arguments.push_back(arg->Evaluate(frame).Result);
	}

	return VMOps::FunctionCall(frame, self, func, arguments);
}

bool FunctionCallExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	JIT_REPLACE_EXPRESSION(m_FName);

	return false;
}

ExpressionResult ArrayExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	Array::Ptr result = new Array();

	BOOST_FOREACH(Expression *aexpr, m_Expressions) {
		result->Add(aexpr->Evaluate(frame).Result);
	}

	return result;
}

bool ArrayExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	EmitJitNewArray(evaluate, res);

	asmjit::X86Mem item = evaluate.newStack(sizeof(Value), 8);

	asmjit::X86GpVar pitem = evaluate.newIntPtr();
	evaluate.lea(pitem, item);

	BOOST_FOREACH(Expression *aexpr, m_Expressions) {
		// XXX/BUG: we might not call the value's dtor if this does 'ret'
		EmitJitExpression(dtor, evaluate, aexpr, frame, dhint, pitem);

		EmitJitArrayAdd(evaluate, res, pitem);
		EmitJitDtorValue(evaluate, pitem);
	}

	m_Expressions.clear();

	delete this;

	return true; // Expression::Compile(owner, dtor, evaluate, frame, dhint, res);
}

ExpressionResult DictExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	Value self;

	if (!m_Inline) {
		self = frame.Self;
		frame.Self = new Dictionary();
	}

	Value result;

	try {
		BOOST_FOREACH(Expression *aexpr, m_Expressions) {
			ExpressionResult element = aexpr->Evaluate(frame, dhint);
			CHECK_RESULT(element);
			result = element.Result;
		}
	} catch (...) {
		if (!m_Inline)
			std::swap(self, frame.Self);
		throw;
	}

	if (m_Inline)
		return result;
	else {
		std::swap(self, frame.Self);
		return self;
	}
}

bool DictExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	asmjit::X86GpVar pres;

	if (!m_Inline) {
		EmitJitNewDictionary(evaluate, res);
		EmitJitScriptFrameSwapSelf(evaluate, frame, res);

		asmjit::X86Mem temp_res = evaluate.newStack(sizeof(Value), 8);

		pres = evaluate.newIntPtr();
		evaluate.lea(pres, temp_res);
	} else {
		pres = res;
	}

	if (m_Expressions.empty())
		EmitJitNewValue(dtor, evaluate, Empty, pres);

	bool first = true;

	BOOST_FOREACH(Expression *aexpr, m_Expressions) {
		if (!first)
			EmitJitDtorValue(evaluate, pres);
		else
			first = false;

		EmitJitExpression(dtor, evaluate, aexpr, frame, dhint, pres);
	}

	m_Expressions.clear();

	if (!m_Inline)
		EmitJitScriptFrameSwapSelf(evaluate, frame, res);

	delete this;

	return true; // Expression::Compile(owner, dtor, evaluate, frame, dhint, res);
}

ExpressionResult GetScopeExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	switch (m_ScopeSpec) {
		case ScopeLocal:
			return frame.Locals;
		case ScopeThis:
			return frame.Self;
		case ScopeGlobal:
			return ScriptGlobal::GetGlobals();
		default:
			VERIFY(!"Invalid scope.");
	}
}

bool GetScopeExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	switch (m_ScopeSpec) {
		case ScopeLocal:
			EmitJitGetScopeLocal(evaluate, frame, res);
			break;
		case ScopeThis:
			EmitJitGetScopeThis(evaluate, frame, res);
			break;
		case ScopeGlobal:
			EmitJitGetScopeGlobal(evaluate, res);
			break;
	}

	delete this;

	return true;
}

ExpressionResult SetExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	if (frame.Sandboxed)
		BOOST_THROW_EXCEPTION(ScriptError("Assignments are not allowed in sandbox mode.", m_DebugInfo));

	DebugHint *psdhint = dhint;

	Value parent;
	String index;

	if (!m_Operand1->GetReference(frame, true, &parent, &index, &psdhint))
		BOOST_THROW_EXCEPTION(ScriptError("Expression cannot be assigned to.", m_DebugInfo));

	ExpressionResult operand2 = m_Operand2->Evaluate(frame, dhint);

	if (m_Op != OpSetLiteral) {
		Value object = VMOps::GetField(parent, index, frame.Sandboxed, m_DebugInfo);

		switch (m_Op) {
			case OpSetAdd:
				operand2 = object + operand2;
				break;
			case OpSetSubtract:
				operand2 = object - operand2;
				break;
			case OpSetMultiply:
				operand2 = object * operand2;
				break;
			case OpSetDivide:
				operand2 = object / operand2;
				break;
			case OpSetModulo:
				operand2 = object % operand2;
				break;
			case OpSetXor:
				operand2 = object ^ operand2;
				break;
			case OpSetBinaryAnd:
				operand2 = object & operand2;
				break;
			case OpSetBinaryOr:
				operand2 = object | operand2;
				break;
			default:
				VERIFY(!"Invalid opcode.");
		}
	}

	VMOps::SetField(parent, index, operand2.Result, m_DebugInfo);

	if (psdhint) {
		psdhint->AddMessage("=", m_DebugInfo);

		if (psdhint != dhint)
			delete psdhint;
	}

	return Empty;
}

bool SetExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	JIT_REPLACE_EXPRESSION(m_Operand1);
	JIT_REPLACE_EXPRESSION(m_Operand2);

	return false;
}

ExpressionResult ConditionalExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	if (m_Condition->Evaluate(frame).Result.ToBool())
		return m_TrueBranch->Evaluate(frame, dhint);
	else if (m_FalseBranch)
		return m_FalseBranch->Evaluate(frame, dhint);

	return Empty;
}

bool ConditionalExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	EmitJitExpression(dtor, evaluate, m_Condition, frame, dhint, res);
	asmjit::X86GpVar eres = EmitJitValueIsTrue(evaluate, res);

	asmjit::Label else_branch = evaluate.newLabel();
	asmjit::Label after_if = evaluate.newLabel();

	evaluate.cmp(eres, 0);
	evaluate.jz(else_branch);

	EmitJitDtorValue(evaluate, res);
	EmitJitExpression(dtor, evaluate, m_TrueBranch, frame, dhint, res);
	evaluate.jmp(after_if);

	evaluate.bind(else_branch);

	EmitJitDtorValue(evaluate, res);

	if (m_FalseBranch)
		EmitJitExpression(dtor, evaluate, m_FalseBranch, frame, dhint, res);
	else
		EmitJitNewValue(dtor, evaluate, Empty, res);

	evaluate.bind(after_if);

	return true;
}

ExpressionResult WhileExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	if (frame.Sandboxed)
		BOOST_THROW_EXCEPTION(ScriptError("While loops are not allowed in sandbox mode.", m_DebugInfo));

	for (;;) {
		if (!m_Condition->Evaluate(frame).Result.ToBool())
			break;

		ExpressionResult loop_body = m_LoopBody->Evaluate(frame, dhint);
		CHECK_RESULT_LOOP(loop_body);
	}

	return Empty;
}

bool WhileExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	asmjit::Label cond_check = evaluate.newLabel();
	asmjit::Label after_loop = evaluate.newLabel();

	evaluate.bind(cond_check);

	EmitJitExpression(dtor, evaluate, m_Condition, frame, dhint, res);
	asmjit::X86GpVar eres = EmitJitValueIsTrue(evaluate, res);

	evaluate.cmp(eres, 0);
	evaluate.jz(after_loop);

	EmitJitDtorValue(evaluate, res);
	EmitJitExpression(dtor, evaluate, m_LoopBody, frame, dhint, res);
	evaluate.jmp(cond_check);

	evaluate.bind(after_loop);

	EmitJitDtorValue(evaluate, res);

	EmitJitNewValue(dtor, evaluate, Empty, res);

	return true;
}

ExpressionResult ReturnExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	ExpressionResult operand = m_Operand->Evaluate(frame);
	operand.Code = ResultReturn;
	return operand;
}

bool ReturnExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	EmitJitExpression(dtor, evaluate, m_Operand, frame, dhint, res);
	m_Operand = NULL;

	evaluate.ret();

	delete this;

	return true;
}

ExpressionResult BreakExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return ExpressionResult(Empty, ResultBreak);
}

bool BreakExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	return false;
}

ExpressionResult ContinueExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return ExpressionResult(Empty, ResultContinue);
}

bool ContinueExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	return false;
}

ExpressionResult IndexerExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return VMOps::GetField(m_Operand1->Evaluate(frame, dhint).Result, m_Operand2->Evaluate(frame, dhint).Result, frame.Sandboxed, m_DebugInfo);
}

bool IndexerExpression::GetReference(ScriptFrame& frame, bool init_dict, Value *parent, String *index, DebugHint **dhint) const
{
	Value vparent;
	String vindex;
	DebugHint *psdhint = NULL;
	bool free_psd = false;

	if (dhint)
		psdhint = *dhint;

	if (frame.Sandboxed)
		init_dict = false;

	if (m_Operand1->GetReference(frame, init_dict, &vparent, &vindex, &psdhint)) {
		if (init_dict) {
			Value old_value =  VMOps::GetField(vparent, vindex, frame.Sandboxed, m_Operand1->GetDebugInfo());

			if (old_value.IsEmpty() && !old_value.IsString())
				VMOps::SetField(vparent, vindex, new Dictionary(), m_Operand1->GetDebugInfo());
		}

		*parent = VMOps::GetField(vparent, vindex, frame.Sandboxed, m_DebugInfo);
		free_psd = true;
	} else {
		ExpressionResult operand1 = m_Operand1->Evaluate(frame);
		*parent = operand1.Result;
	}

	ExpressionResult operand2 = m_Operand2->Evaluate(frame);
	*index = operand2.Result;

	if (dhint) {
		if (psdhint)
			*dhint = new DebugHint(psdhint->GetChild(*index));
		else
			*dhint = NULL;
	}

	if (free_psd)
		delete psdhint;

	return true;
}

bool IndexerExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	JIT_REPLACE_EXPRESSION(m_Operand1);
	JIT_REPLACE_EXPRESSION(m_Operand2);

	return false;
}

void icinga::BindToScope(Expression *& expr, ScopeSpecifier scopeSpec)
{
	ASSERT(!dynamic_cast<JitExpression *>(expr));

	DictExpression *dexpr = dynamic_cast<DictExpression *>(expr);

	if (dexpr) {
		BOOST_FOREACH(Expression *& expr, dexpr->m_Expressions)
			BindToScope(expr, scopeSpec);

		return;
	}

	SetExpression *aexpr = dynamic_cast<SetExpression *>(expr);

	if (aexpr) {
		BindToScope(aexpr->m_Operand1, scopeSpec);

		return;
	}

	IndexerExpression *iexpr = dynamic_cast<IndexerExpression *>(expr);

	if (iexpr) {
		BindToScope(iexpr->m_Operand1, scopeSpec);
		return;
	}

	LiteralExpression *lexpr = dynamic_cast<LiteralExpression *>(expr);
	ScriptFrame frame;

	if (lexpr && lexpr->Evaluate(frame).Result.IsString()) {
		Expression *scope = new GetScopeExpression(scopeSpec);
		expr = new IndexerExpression(scope, lexpr, lexpr->GetDebugInfo());
	}

	VariableExpression *vexpr = dynamic_cast<VariableExpression *>(expr);

	if (vexpr) {
		Expression *scope = new GetScopeExpression(scopeSpec);
		Expression *new_expr = new IndexerExpression(scope, MakeLiteral(vexpr->GetVariable()), vexpr->GetDebugInfo());
		delete expr;
		expr = new_expr;
	}
}

ExpressionResult ThrowExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	BOOST_THROW_EXCEPTION(ScriptError(m_Message->Evaluate(frame).Result, m_DebugInfo, m_IncompleteExpr));
}

bool ThrowExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	JIT_REPLACE_EXPRESSION(m_Message);

	return false;
}

ExpressionResult ImportExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	if (frame.Sandboxed)
		BOOST_THROW_EXCEPTION(ScriptError("Imports are not allowed in sandbox mode.", m_DebugInfo));

	String type = VMOps::GetField(frame.Self, "type", frame.Sandboxed, m_DebugInfo);
	Value name = m_Name->Evaluate(frame).Result;

	if (!name.IsString())
		BOOST_THROW_EXCEPTION(ScriptError("Template/object name must be a string", m_DebugInfo));

	ConfigItem::Ptr item = ConfigItem::GetByTypeAndName(type, name);

	if (!item)
		BOOST_THROW_EXCEPTION(ScriptError("Import references unknown template: '" + name + "'", m_DebugInfo));

	return item->GetExpression()->Evaluate(frame, dhint).Result;
}

bool ImportExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	JIT_REPLACE_EXPRESSION(m_Name);

	return false;
}

ExpressionResult FunctionExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	return VMOps::NewFunction(frame, m_Args, m_ClosedVars, m_Expression);
}

bool FunctionExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	if (m_ClosedVars) {
		typedef std::map<String, Expression *>::value_type kv_pair;
		BOOST_FOREACH(kv_pair& kv, *m_ClosedVars) {
			JIT_REPLACE_EXPRESSION(kv.second);
		}
	}

	// TODO: m_Expression

	return false;
}

ExpressionResult ApplyExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	if (frame.Sandboxed)
		BOOST_THROW_EXCEPTION(ScriptError("Apply rules are not allowed in sandbox mode.", m_DebugInfo));

	return VMOps::NewApply(frame, m_Type, m_Target, m_Name->Evaluate(frame).Result, m_Filter,
	    m_Package, m_FKVar, m_FVVar, m_FTerm, m_ClosedVars, m_IgnoreOnError, m_Expression, m_DebugInfo);
}

bool ApplyExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	JIT_REPLACE_EXPRESSION(m_Name);

	if (m_ClosedVars) {
		typedef std::map<String, Expression *>::value_type kv_pair;
		BOOST_FOREACH(kv_pair& kv, *m_ClosedVars) {
			JIT_REPLACE_EXPRESSION(kv.second);
		}
	}
	// TODO: m_Filter
	// TODO: m_FTerm
	// TODO: m_Expression

	return false;
}

ExpressionResult ObjectExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	if (frame.Sandboxed)
		BOOST_THROW_EXCEPTION(ScriptError("Object definitions are not allowed in sandbox mode.", m_DebugInfo));

	String name;

	if (m_Name)
		name = m_Name->Evaluate(frame).Result;

	return VMOps::NewObject(frame, m_Abstract, m_Type, name, m_Filter, m_Zone,
	    m_Package, m_IgnoreOnError, m_ClosedVars, m_Expression, m_DebugInfo);
}

bool ObjectExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	JIT_REPLACE_EXPRESSION(m_Name);

	if (m_ClosedVars) {
		typedef std::map<String, Expression *>::value_type kv_pair;
		BOOST_FOREACH(kv_pair& kv, *m_ClosedVars) {
			JIT_REPLACE_EXPRESSION(kv.second);
		}
	}

	// TODO: m_Filter
	// TODO: m_Expression

	return false;
}

ExpressionResult ForExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	if (frame.Sandboxed)
		BOOST_THROW_EXCEPTION(ScriptError("For loops are not allowed in sandbox mode.", m_DebugInfo));

	return VMOps::For(frame, m_FKVar, m_FVVar, m_Value->Evaluate(frame, dhint).Result, m_Expression, m_DebugInfo);
}

bool ForExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	JIT_REPLACE_EXPRESSION(m_Value);
	JIT_REPLACE_EXPRESSION(m_Expression);

	return false;
}

ExpressionResult LibraryExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	if (frame.Sandboxed)
		BOOST_THROW_EXCEPTION(ScriptError("Loading libraries is not allowed in sandbox mode.", m_DebugInfo));

	Loader::LoadExtensionLibrary(m_Operand->Evaluate(frame).Result);

	return Empty;
}

bool LibraryExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	return false;
}

ExpressionResult IncludeExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	if (frame.Sandboxed)
		BOOST_THROW_EXCEPTION(ScriptError("Includes are not allowed in sandbox mode.", m_DebugInfo));

	Expression *expr;
	String name, path, pattern;

	switch (m_Type) {
		case IncludeRegular:
			path = m_Path->Evaluate(frame).Result;
			expr = ConfigCompiler::HandleInclude(m_RelativeBase, path, m_SearchIncludes, m_Zone, m_Package, m_DebugInfo);
			break;

		case IncludeRecursive:
			path = m_Path->Evaluate(frame).Result;
			pattern = m_Pattern->Evaluate(frame).Result;
			expr = ConfigCompiler::HandleIncludeRecursive(m_RelativeBase, path, pattern, m_Zone, m_Package, m_DebugInfo);
			break;

		case IncludeZones:
			name = m_Name->Evaluate(frame).Result;
			path = m_Path->Evaluate(frame).Result;
			pattern = m_Pattern->Evaluate(frame).Result;
			expr = ConfigCompiler::HandleIncludeZones(m_RelativeBase, name, path, pattern, m_Package, m_DebugInfo);
			break;
	}

	ExpressionResult res(Empty);

	try {
		res = expr->Evaluate(frame, dhint);
	} catch (const std::exception&) {
		delete expr;
		throw;
	}

	delete expr;

	return res;
}

bool IncludeExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	JIT_REPLACE_EXPRESSION(m_Path);
	JIT_REPLACE_EXPRESSION(m_Pattern);
	JIT_REPLACE_EXPRESSION(m_Name);

	return false;
}

ExpressionResult BreakpointExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	ScriptBreakpoint(frame, NULL, GetDebugInfo());

	return Empty;
}

bool BreakpointExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	return false;
}

bool Expression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	EmitJitDeleteExpression(dtor, this);

	EmitJitInvokeDoEvaluate(evaluate, this, frame, dhint, res);

	return true;
}

JitExpression::JitExpression(Expression *otherExpression)
{
	asmjit::X86Assembler assemblerDtor(&m_Runtime);
	asmjit::X86Assembler assemblerEvaluate(&m_Runtime);
	asmjit::X86Compiler compilerDtor(&assemblerDtor);
	asmjit::X86Compiler compilerEvaluate(&assemblerEvaluate);

	compilerDtor.addFunc(asmjit::FuncBuilder0<void>(asmjit::kCallConvHost));
	compilerEvaluate.addFunc(asmjit::FuncBuilder3<void, ScriptFrame *, DebugHint *, ExpressionResult *>(asmjit::kCallConvHost));

	asmjit::GpVar frame = compilerEvaluate.newIntPtr();
	asmjit::GpVar dhint = compilerEvaluate.newIntPtr();
	asmjit::GpVar res = compilerEvaluate.newIntPtr();

	compilerEvaluate.setArg(0, frame);
	compilerEvaluate.setArg(1, dhint);
	compilerEvaluate.setArg(2, res);

	if (!otherExpression->Compile(compilerDtor, compilerEvaluate, frame, dhint, res))
		BOOST_THROW_EXCEPTION(std::invalid_argument("Expression doesn't support JIT compilation"));

	compilerDtor.ret();
	compilerDtor.endFunc();
	compilerDtor.finalize();

	m_Dtor = asmjit_cast<JitDtor>(assemblerDtor.make());

	compilerEvaluate.ret();
	compilerEvaluate.endFunc();
	compilerEvaluate.finalize();

	m_Evaluate = asmjit_cast<JitEvaluateFunction>(assemblerEvaluate.make());
}

JitExpression::~JitExpression(void)
{
	m_Dtor();
}

bool JitExpression::Compile(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	return false;
}

ExpressionResult JitExpression::DoEvaluate(ScriptFrame& frame, DebugHint *dhint) const
{
	char res[sizeof(Value)];
	Value *pres = reinterpret_cast<Value *>(res);
	m_Evaluate(&frame, dhint, pres);
	return *pres;
}
