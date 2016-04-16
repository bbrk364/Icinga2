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

#ifndef JITOPS_H
#define JITOPS_H

#include "config/i2-config.hpp"
#include "config/expression.hpp"
#include "config/configitembuilder.hpp"
#include "config/applyrule.hpp"
#include "config/objectrule.hpp"
#include "base/debuginfo.hpp"
#include "base/array.hpp"
#include "base/dictionary.hpp"
#include "base/function.hpp"
#include "base/scriptglobal.hpp"
#include "base/exception.hpp"
#include "base/convert.hpp"
#include "base/objectlock.hpp"
#include <boost/foreach.hpp>
#include <boost/smart_ptr/make_shared.hpp>
#include <map>
#include <vector>

namespace icinga
{

inline void JitNewValueEmpty(Value *res)
{
	new (res) Value();
}

inline void JitNewValueNumber(double val, Value *res)
{
	new (res) Value(val);
}

inline void JitNewValueBoolean(int val, Value *res)
{
	new (res) Value(static_cast<bool>(val));
}

inline void JitNewValueString(const char *val, size_t len, Value *res)
{
	new (res) Value(String(val, val + len));
}

inline void JitNewValueObject(Object *val, Value *res)
{
	new (res) Value(val);
}

inline void JitNewDictionary(Value *res)
{
	new (res) Value(new Dictionary());
}

inline void EmitJitNewDictionary(asmjit::X86Compiler& compiler, asmjit::X86GpVar& val)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitNewDictionary), asmjit::FuncBuilder1<void, Value *>(asmjit::kCallConvHost));
	call->setArg(0, val);
}

inline void JitScriptFrameSwapSelf(ScriptFrame *frame, Value *self)
{
	std::swap(frame->Self, *self);
}

inline void EmitJitScriptFrameSwapSelf(asmjit::X86Compiler& compiler, asmjit::X86GpVar& frame, asmjit::X86GpVar& self)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitScriptFrameSwapSelf), asmjit::FuncBuilder2<void, ScriptFrame *, Value *>(asmjit::kCallConvHost));
	call->setArg(0, frame);
	call->setArg(1, self);
}

inline void JitNewArray(Value *res)
{
	new (res) Value(new Array());
}

inline void EmitJitNewArray(asmjit::X86Compiler& compiler, asmjit::X86GpVar& val)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitNewArray), asmjit::FuncBuilder1<void, Value *>(asmjit::kCallConvHost));
	call->setArg(0, val);
}

inline void JitArrayAdd(Value *array, Value *item)
{
	static_cast<Array::Ptr>(*array)->Add(*item);
}

inline void EmitJitArrayAdd(asmjit::X86Compiler& compiler, asmjit::X86GpVar& array, asmjit::X86GpVar& val)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitArrayAdd), asmjit::FuncBuilder2<void, Value *, Value *>(asmjit::kCallConvHost));
	call->setArg(0, array);
	call->setArg(1, val);
}

inline int32_t JitValueIsTrue(Value *val)
{
	return val->ToBool();
}

inline asmjit::X86GpVar EmitJitValueIsTrue(asmjit::X86Compiler& compiler, asmjit::X86GpVar& val)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitValueIsTrue), asmjit::FuncBuilder1<void, Value *>(asmjit::kCallConvHost));
	call->setArg(0, val);

	asmjit::X86GpVar eres = compiler.newInt32();
	call->setRet(0, eres);

	return eres;
}

inline void JitDtorValue(Value *val)
{
	val->~Value();
}

inline void EmitJitDtorValue(asmjit::X86Compiler& compiler, asmjit::X86GpVar& val)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitDtorValue), asmjit::FuncBuilder1<void, Value *>(asmjit::kCallConvHost));
	call->setArg(0, val);
}

inline int32_t JitInvokeDoEvaluate(Expression *expr, ScriptFrame *frame, DebugHint *dhint, Value *res)
{
	ExpressionResult eres = expr->DoEvaluate(*frame, dhint);

	if (eres.GetCode() != ResultOK) {
		new (res) Value();
		return 0;
	}

	new (res) Value(eres.GetValue());

	return 1;
}

inline void JitDeleteExpression(Expression *expr)
{
	delete expr;
}

inline void EmitJitDeleteExpression(asmjit::X86Compiler& compiler, Expression *expr)
{
	asmjit::X86CallNode *dcall = compiler.addCall(asmjit::imm_ptr(&JitDeleteExpression), asmjit::FuncBuilder1<void, Expression *>(asmjit::kCallConvHost));
	dcall->setArg(0, asmjit::imm_ptr(expr));
}

inline void EmitJitInvokeDoEvaluate(asmjit::X86Compiler& compiler, Expression *expr,
	asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	asmjit::X86GpVar eres = compiler.newInt32();

	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitInvokeDoEvaluate),
		asmjit::FuncBuilder4<int32_t, Expression *, ScriptFrame *, DebugHint *, Value *>(asmjit::kCallConvHost));
	call->setArg(0, asmjit::imm_ptr(expr));
	call->setArg(1, frame);
	call->setArg(2, dhint);
	call->setArg(3, res);
	call->setRet(0, eres);

	asmjit::Label after_if = compiler.newLabel();

	compiler.cmp(eres, 0);
	compiler.jnz(after_if);

	compiler.ret();

	compiler.bind(after_if);
}

inline void EmitJitExpression(asmjit::X86Compiler& dtor, asmjit::X86Compiler& evaluate, Expression *expr,
	asmjit::X86GpVar& frame, asmjit::X86GpVar& dhint, asmjit::X86GpVar& res)
{
	if (!expr->Compile(dtor, evaluate, frame, dhint, res)) {
		EmitJitInvokeDoEvaluate(evaluate, expr, frame, dhint, res);
		EmitJitDeleteExpression(dtor, expr);
	}
}

inline void EmitJitNewValue(asmjit::X86Compiler& compilerDtor, asmjit::X86Compiler& compilerEvaluate, const Value& value, asmjit::X86GpVar& res)
{
	asmjit::X86CallNode *call;

	switch (value.GetType()) {
		case ValueEmpty:
		{
			call = compilerEvaluate.addCall(asmjit::imm_ptr(&JitNewValueEmpty), asmjit::FuncBuilder1<void, Value *>(asmjit::kCallConvHost));
			call->setArg(0, res);

			break;
		}

		case ValueNumber:
		{
			double num = value;
			asmjit::X86Mem val = compilerEvaluate.newDoubleConst(asmjit::kConstScopeLocal, value);

			asmjit::X86XmmVar pval = compilerEvaluate.newXmmSd();
			compilerEvaluate.movsd(pval, val);

			call = compilerEvaluate.addCall(asmjit::imm_ptr(&JitNewValueNumber), asmjit::FuncBuilder2<void, double, Value *>(asmjit::kCallConvHost));
			call->setArg(0, pval);
			call->setArg(1, res);

			break;
		}

		case ValueBoolean:
		{
			call = compilerEvaluate.addCall(asmjit::imm_ptr(&JitNewValueBoolean), asmjit::FuncBuilder2<void, int, Value *>(asmjit::kCallConvHost));
			call->setArg(0, asmjit::imm(static_cast<bool>(value)));
			call->setArg(1, res);

			break;
		}

		case ValueString:
		{
			String str = value;

			char *strd = (char *)malloc(str.GetLength());

			if (!strd)
				BOOST_THROW_EXCEPTION(std::bad_alloc());

			memcpy(strd, str.CStr(), str.GetLength());

			call = compilerEvaluate.addCall(asmjit::imm_ptr(&JitNewValueString), asmjit::FuncBuilder3<void, const char *, size_t, Value *>(asmjit::kCallConvHost));
			call->setArg(0, asmjit::imm_ptr(strd));
			call->setArg(1, asmjit::imm(str.GetLength()));
			call->setArg(2, res);

			asmjit::X86CallNode *dcall = compilerDtor.addCall(asmjit::imm_ptr(free), asmjit::FuncBuilder1<void, void *>(asmjit::kCallConvHost));
			dcall->setArg(0, asmjit::imm_ptr(strd));

			break;
		}

		case ValueObject:
		default:
			VERIFY(!"Unsupported Value type for JIT.");
	}
}

inline void JitValueNegate(Value& val)
{
	val = ~(long)val;
}

inline void EmitJitValueNegate(asmjit::X86Compiler& compiler, asmjit::X86GpVar& val)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitValueNegate), asmjit::FuncBuilder1<void, Value *>(asmjit::kCallConvHost));
	call->setArg(0, val);
}

inline void JitValueLogicalNegate(Value& val)
{
	val = !val.ToBool();
}

inline void EmitJitValueLogicalNegate(asmjit::X86Compiler& compiler, asmjit::X86GpVar& val)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitValueLogicalNegate), asmjit::FuncBuilder1<void, Value *>(asmjit::kCallConvHost));
	call->setArg(0, val);
}

inline void JitValueAdd(const Value& op1, Value& op2out)
{
	op2out = op1 + op2out;
}

inline void EmitJitValueAdd(asmjit::X86Compiler& compiler, asmjit::X86GpVar& op1, asmjit::X86GpVar& op2out)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitValueAdd), asmjit::FuncBuilder2<void, Value *, Value *>(asmjit::kCallConvHost));
	call->setArg(0, op1);
	call->setArg(1, op2out);
}

inline void JitValueSubtract(const Value& op1, Value& op2out)
{
	op2out = op1 - op2out;
}

inline void EmitJitValueSubtract(asmjit::X86Compiler& compiler, asmjit::X86GpVar& op1, asmjit::X86GpVar& op2out)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitValueSubtract), asmjit::FuncBuilder2<void, Value *, Value *>(asmjit::kCallConvHost));
	call->setArg(0, op1);
	call->setArg(1, op2out);
}

inline void JitValueMultiply(const Value& op1, Value& op2out)
{
	op2out = op1 * op2out;
}

inline void EmitJitValueMultiply(asmjit::X86Compiler& compiler, asmjit::X86GpVar& op1, asmjit::X86GpVar& op2out)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitValueMultiply), asmjit::FuncBuilder2<void, Value *, Value *>(asmjit::kCallConvHost));
	call->setArg(0, op1);
	call->setArg(1, op2out);
}

inline void JitValueDivide(const Value& op1, Value& op2out)
{
	op2out = op1 / op2out;
}

inline void EmitJitValueDivide(asmjit::X86Compiler& compiler, asmjit::X86GpVar& op1, asmjit::X86GpVar& op2out)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitValueDivide), asmjit::FuncBuilder2<void, Value *, Value *>(asmjit::kCallConvHost));
	call->setArg(0, op1);
	call->setArg(1, op2out);
}

inline void JitValueModulo(const Value& op1, Value& op2out)
{
	op2out = op1 % op2out;
}

inline void EmitJitValueModulo(asmjit::X86Compiler& compiler, asmjit::X86GpVar& op1, asmjit::X86GpVar& op2out)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitValueModulo), asmjit::FuncBuilder2<void, Value *, Value *>(asmjit::kCallConvHost));
	call->setArg(0, op1);
	call->setArg(1, op2out);
}

inline void JitValueXor(const Value& op1, Value& op2out)
{
	op2out = op1 ^ op2out;
}

inline void EmitJitValueXor(asmjit::X86Compiler& compiler, asmjit::X86GpVar& op1, asmjit::X86GpVar& op2out)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitValueXor), asmjit::FuncBuilder2<void, Value *, Value *>(asmjit::kCallConvHost));
	call->setArg(0, op1);
	call->setArg(1, op2out);
}

inline void JitValueBinaryAnd(const Value& op1, Value& op2out)
{
	op2out = op1 & op2out;
}

inline void EmitJitValueBinaryAnd(asmjit::X86Compiler& compiler, asmjit::X86GpVar& op1, asmjit::X86GpVar& op2out)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitValueBinaryAnd), asmjit::FuncBuilder2<void, Value *, Value *>(asmjit::kCallConvHost));
	call->setArg(0, op1);
	call->setArg(1, op2out);
}

inline void JitValueBinaryOr(const Value& op1, Value& op2out)
{
	op2out = op1 | op2out;
}

inline void EmitJitValueBinaryOr(asmjit::X86Compiler& compiler, asmjit::X86GpVar& op1, asmjit::X86GpVar& op2out)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitValueBinaryOr), asmjit::FuncBuilder2<void, Value *, Value *>(asmjit::kCallConvHost));
	call->setArg(0, op1);
	call->setArg(1, op2out);
}

inline void JitValueBinaryShiftLeft(const Value& op1, Value& op2out)
{
	op2out = op1 << op2out;
}

inline void EmitJitValueShiftLeft(asmjit::X86Compiler& compiler, asmjit::X86GpVar& op1, asmjit::X86GpVar& op2out)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitValueBinaryShiftLeft), asmjit::FuncBuilder2<void, Value *, Value *>(asmjit::kCallConvHost));
	call->setArg(0, op1);
	call->setArg(1, op2out);
}

inline void JitValueBinaryShiftRight(const Value& op1, Value& op2out)
{
	op2out = op1 >> op2out;
}

inline void EmitJitValueShiftRight(asmjit::X86Compiler& compiler, asmjit::X86GpVar& op1, asmjit::X86GpVar& op2out)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitValueBinaryShiftRight), asmjit::FuncBuilder2<void, Value *, Value *>(asmjit::kCallConvHost));
	call->setArg(0, op1);
	call->setArg(1, op2out);
}

inline void JitValueEqual(const Value& op1, Value& op2out)
{
	op2out = op1 == op2out;
}

inline void EmitJitValueEqual(asmjit::X86Compiler& compiler, asmjit::X86GpVar& op1, asmjit::X86GpVar& op2out)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitValueEqual), asmjit::FuncBuilder2<void, Value *, Value *>(asmjit::kCallConvHost));
	call->setArg(0, op1);
	call->setArg(1, op2out);
}

inline void JitValueNotEqual(const Value& op1, Value& op2out)
{
	op2out = op1 != op2out;
}

inline void EmitJitValueNotEqual(asmjit::X86Compiler& compiler, asmjit::X86GpVar& op1, asmjit::X86GpVar& op2out)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitValueNotEqual), asmjit::FuncBuilder2<void, Value *, Value *>(asmjit::kCallConvHost));
	call->setArg(0, op1);
	call->setArg(1, op2out);
}

inline void JitValueLessThan(const Value& op1, Value& op2out)
{
	op2out = op1 < op2out;
}

inline void EmitJitValueLessThan(asmjit::X86Compiler& compiler, asmjit::X86GpVar& op1, asmjit::X86GpVar& op2out)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitValueLessThan), asmjit::FuncBuilder2<void, Value *, Value *>(asmjit::kCallConvHost));
	call->setArg(0, op1);
	call->setArg(1, op2out);
}

inline void JitValueGreaterThan(const Value& op1, Value& op2out)
{
	op2out = op1 > op2out;
}

inline void EmitJitValueGreaterThan(asmjit::X86Compiler& compiler, asmjit::X86GpVar& op1, asmjit::X86GpVar& op2out)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitValueGreaterThan), asmjit::FuncBuilder2<void, Value *, Value *>(asmjit::kCallConvHost));
	call->setArg(0, op1);
	call->setArg(1, op2out);
}

inline void JitValueLessThanOrEqual(const Value& op1, Value& op2out)
{
	op2out = op1 <= op2out;
}

inline void EmitJitValueLessThanOrEqual(asmjit::X86Compiler& compiler, asmjit::X86GpVar& op1, asmjit::X86GpVar& op2out)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitValueLessThanOrEqual), asmjit::FuncBuilder2<void, Value *, Value *>(asmjit::kCallConvHost));
	call->setArg(0, op1);
	call->setArg(1, op2out);
}

inline void JitValueGreaterThanOrEqual(const Value& op1, Value& op2out)
{
	op2out = op1 >= op2out;
}

inline void EmitJitValueGreaterThanOrEqual(asmjit::X86Compiler& compiler, asmjit::X86GpVar& op1, asmjit::X86GpVar& op2out)
{
	asmjit::X86CallNode *call = compiler.addCall(asmjit::imm_ptr(&JitValueGreaterThanOrEqual), asmjit::FuncBuilder2<void, Value *, Value *>(asmjit::kCallConvHost));
	call->setArg(0, op1);
	call->setArg(1, op2out);
}

}

#endif /* JITOPS_H */
