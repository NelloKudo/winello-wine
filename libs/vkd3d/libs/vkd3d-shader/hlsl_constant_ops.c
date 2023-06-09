/*
 * HLSL constant value operations for constant folding
 *
 * Copyright 2022 Francisco Casas for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <math.h>

#include "hlsl.h"

static bool fold_abs(struct hlsl_ctx *ctx, struct hlsl_constant_value *dst,
        const struct hlsl_type *dst_type, const struct hlsl_ir_constant *src)
{
    enum hlsl_base_type type = dst_type->base_type;
    unsigned int k;

    assert(type == src->node.data_type->base_type);

    for (k = 0; k < dst_type->dimx; ++k)
    {
        switch (type)
        {
            case HLSL_TYPE_FLOAT:
            case HLSL_TYPE_HALF:
                dst->u[k].f = fabsf(src->value.u[k].f);
                break;

            case HLSL_TYPE_DOUBLE:
                dst->u[k].d = fabs(src->value.u[k].d);
                break;

            case HLSL_TYPE_INT:
                /* C's abs(INT_MIN) is undefined, but HLSL evaluates this to INT_MIN */
                if (src->value.u[k].i == INT_MIN)
                    dst->u[k].i = INT_MIN;
                else
                    dst->u[k].i = abs(src->value.u[k].i);
                break;

            case HLSL_TYPE_UINT:
                dst->u[k].u = src->value.u[k].u;
                break;

            default:
                FIXME("Fold abs() for type %s.\n", debug_hlsl_type(ctx, dst_type));
                return false;
        }
    }
    return true;
}

static bool fold_cast(struct hlsl_ctx *ctx, struct hlsl_constant_value *dst,
        const struct hlsl_type *dst_type, const struct hlsl_ir_constant *src)
{
    unsigned int k;
    uint32_t u;
    int32_t i;
    double d;
    float f;

    if (dst_type->dimx != src->node.data_type->dimx
            || dst_type->dimy != src->node.data_type->dimy)
    {
        FIXME("Cast from %s to %s.\n", debug_hlsl_type(ctx, src->node.data_type),
                debug_hlsl_type(ctx, dst_type));
        return false;
    }

    for (k = 0; k < 4; ++k)
    {
        switch (src->node.data_type->base_type)
        {
            case HLSL_TYPE_FLOAT:
            case HLSL_TYPE_HALF:
                u = src->value.u[k].f;
                i = src->value.u[k].f;
                f = src->value.u[k].f;
                d = src->value.u[k].f;
                break;

            case HLSL_TYPE_DOUBLE:
                u = src->value.u[k].d;
                i = src->value.u[k].d;
                f = src->value.u[k].d;
                d = src->value.u[k].d;
                break;

            case HLSL_TYPE_INT:
                u = src->value.u[k].i;
                i = src->value.u[k].i;
                f = src->value.u[k].i;
                d = src->value.u[k].i;
                break;

            case HLSL_TYPE_UINT:
                u = src->value.u[k].u;
                i = src->value.u[k].u;
                f = src->value.u[k].u;
                d = src->value.u[k].u;
                break;

            case HLSL_TYPE_BOOL:
                u = !!src->value.u[k].u;
                i = !!src->value.u[k].u;
                f = !!src->value.u[k].u;
                d = !!src->value.u[k].u;
                break;

            default:
                vkd3d_unreachable();
        }

        switch (dst_type->base_type)
        {
            case HLSL_TYPE_FLOAT:
            case HLSL_TYPE_HALF:
                dst->u[k].f = f;
                break;

            case HLSL_TYPE_DOUBLE:
                dst->u[k].d = d;
                break;

            case HLSL_TYPE_INT:
                dst->u[k].i = i;
                break;

            case HLSL_TYPE_UINT:
                dst->u[k].u = u;
                break;

            case HLSL_TYPE_BOOL:
                /* Casts to bool should have already been lowered. */
            default:
                vkd3d_unreachable();
        }
    }
    return true;
}

static bool fold_neg(struct hlsl_ctx *ctx, struct hlsl_constant_value *dst,
        const struct hlsl_type *dst_type, const struct hlsl_ir_constant *src)
{
    enum hlsl_base_type type = dst_type->base_type;
    unsigned int k;

    assert(type == src->node.data_type->base_type);

    for (k = 0; k < 4; ++k)
    {
        switch (type)
        {
            case HLSL_TYPE_FLOAT:
            case HLSL_TYPE_HALF:
                dst->u[k].f = -src->value.u[k].f;
                break;

            case HLSL_TYPE_DOUBLE:
                dst->u[k].d = -src->value.u[k].d;
                break;

            case HLSL_TYPE_INT:
            case HLSL_TYPE_UINT:
                dst->u[k].u = -src->value.u[k].u;
                break;

            default:
                FIXME("Fold negation for type %s.\n", debug_hlsl_type(ctx, dst_type));
                return false;
        }
    }
    return true;
}

static bool fold_add(struct hlsl_ctx *ctx, struct hlsl_constant_value *dst, const struct hlsl_type *dst_type,
        const struct hlsl_ir_constant *src1, const struct hlsl_ir_constant *src2)
{
    enum hlsl_base_type type = dst_type->base_type;
    unsigned int k;

    assert(type == src1->node.data_type->base_type);
    assert(type == src2->node.data_type->base_type);

    for (k = 0; k < 4; ++k)
    {
        switch (type)
        {
            case HLSL_TYPE_FLOAT:
            case HLSL_TYPE_HALF:
                dst->u[k].f = src1->value.u[k].f + src2->value.u[k].f;
                break;

            case HLSL_TYPE_DOUBLE:
                dst->u[k].d = src1->value.u[k].d + src2->value.u[k].d;
                break;

            /* Handling HLSL_TYPE_INT through the unsigned field to avoid
             * undefined behavior with signed integers in C. */
            case HLSL_TYPE_INT:
            case HLSL_TYPE_UINT:
                dst->u[k].u = src1->value.u[k].u + src2->value.u[k].u;
                break;

            default:
                FIXME("Fold addition for type %s.\n", debug_hlsl_type(ctx, dst_type));
                return false;
        }
    }
    return true;
}

static bool fold_mul(struct hlsl_ctx *ctx, struct hlsl_constant_value *dst, const struct hlsl_type *dst_type,
        const struct hlsl_ir_constant *src1, const struct hlsl_ir_constant *src2)
{
    enum hlsl_base_type type = dst_type->base_type;
    unsigned int k;

    assert(type == src1->node.data_type->base_type);
    assert(type == src2->node.data_type->base_type);

    for (k = 0; k < 4; ++k)
    {
        switch (type)
        {
            case HLSL_TYPE_FLOAT:
            case HLSL_TYPE_HALF:
                dst->u[k].f = src1->value.u[k].f * src2->value.u[k].f;
                break;

            case HLSL_TYPE_DOUBLE:
                dst->u[k].d = src1->value.u[k].d * src2->value.u[k].d;
                break;

            case HLSL_TYPE_INT:
            case HLSL_TYPE_UINT:
                dst->u[k].u = src1->value.u[k].u * src2->value.u[k].u;
                break;

            default:
                FIXME("Fold multiplication for type %s.\n", debug_hlsl_type(ctx, dst_type));
                return false;
        }
    }
    return true;
}

static bool fold_nequal(struct hlsl_ctx *ctx, struct hlsl_constant_value *dst, const struct hlsl_type *dst_type,
        const struct hlsl_ir_constant *src1, const struct hlsl_ir_constant *src2)
{
    unsigned int k;

    assert(dst_type->base_type == HLSL_TYPE_BOOL);
    assert(src1->node.data_type->base_type == src2->node.data_type->base_type);

    for (k = 0; k < 4; ++k)
    {
        switch (src1->node.data_type->base_type)
        {
            case HLSL_TYPE_FLOAT:
            case HLSL_TYPE_HALF:
                dst->u[k].u = src1->value.u[k].f != src2->value.u[k].f;
                break;

            case HLSL_TYPE_DOUBLE:
                dst->u[k].u = src1->value.u[k].d != src2->value.u[k].d;
                break;

            case HLSL_TYPE_INT:
            case HLSL_TYPE_UINT:
            case HLSL_TYPE_BOOL:
                dst->u[k].u = src1->value.u[k].u != src2->value.u[k].u;
                break;

            default:
                vkd3d_unreachable();
        }

        dst->u[k].u *= ~0u;
    }
    return true;
}

static bool fold_div(struct hlsl_ctx *ctx, struct hlsl_constant_value *dst, const struct hlsl_type *dst_type,
        const struct hlsl_ir_constant *src1, const struct hlsl_ir_constant *src2,
        const struct vkd3d_shader_location *loc)
{
    enum hlsl_base_type type = dst_type->base_type;
    unsigned int k;

    assert(type == src1->node.data_type->base_type);
    assert(type == src2->node.data_type->base_type);

    for (k = 0; k < dst_type->dimx; ++k)
    {
        switch (type)
        {
            case HLSL_TYPE_FLOAT:
            case HLSL_TYPE_HALF:
                if (ctx->profile->major_version >= 4 && src2->value.u[k].f == 0)
                {
                    hlsl_warning(ctx, loc, VKD3D_SHADER_WARNING_HLSL_DIVISION_BY_ZERO,
                            "Floating point division by zero.");
                }
                dst->u[k].f = src1->value.u[k].f / src2->value.u[k].f;
                if (ctx->profile->major_version < 4 && !isfinite(dst->u[k].f))
                {
                    hlsl_error(ctx, loc, VKD3D_SHADER_ERROR_HLSL_DIVISION_BY_ZERO,
                            "Infinities and NaNs are not allowed by the shader model.");
                }
                break;

            case HLSL_TYPE_DOUBLE:
                if (src2->value.u[k].d == 0)
                {
                    hlsl_warning(ctx, loc, VKD3D_SHADER_WARNING_HLSL_DIVISION_BY_ZERO,
                            "Floating point division by zero.");
                }
                dst->u[k].d = src1->value.u[k].d / src2->value.u[k].d;
                break;

            case HLSL_TYPE_INT:
                if (src2->value.u[k].i == 0)
                {
                    hlsl_error(ctx, loc, VKD3D_SHADER_ERROR_HLSL_DIVISION_BY_ZERO,
                            "Division by zero.");
                    return false;
                }
                if (src1->value.u[k].i == INT_MIN && src2->value.u[k].i == -1)
                    dst->u[k].i = INT_MIN;
                else
                    dst->u[k].i = src1->value.u[k].i / src2->value.u[k].i;
                break;

            case HLSL_TYPE_UINT:
                if (src2->value.u[k].u == 0)
                {
                    hlsl_error(ctx, loc, VKD3D_SHADER_ERROR_HLSL_DIVISION_BY_ZERO,
                            "Division by zero.");
                    return false;
                }
                dst->u[k].u = src1->value.u[k].u / src2->value.u[k].u;
                break;

            default:
                FIXME("Fold division for type %s.\n", debug_hlsl_type(ctx, dst_type));
                return false;
        }
    }
    return true;
}

static bool fold_mod(struct hlsl_ctx *ctx, struct hlsl_constant_value *dst, const struct hlsl_type *dst_type,
        const struct hlsl_ir_constant *src1, const struct hlsl_ir_constant *src2,
        const struct vkd3d_shader_location *loc)
{
    enum hlsl_base_type type = dst_type->base_type;
    unsigned int k;

    assert(type == src1->node.data_type->base_type);
    assert(type == src2->node.data_type->base_type);

    for (k = 0; k < dst_type->dimx; ++k)
    {
        switch (type)
        {
            case HLSL_TYPE_INT:
                if (src2->value.u[k].i == 0)
                {
                    hlsl_error(ctx, loc, VKD3D_SHADER_ERROR_HLSL_DIVISION_BY_ZERO, "Division by zero.");
                    return false;
                }
                if (src1->value.u[k].i == INT_MIN && src2->value.u[k].i == -1)
                    dst->u[k].i = 0;
                else
                    dst->u[k].i = src1->value.u[k].i % src2->value.u[k].i;
                break;

            case HLSL_TYPE_UINT:
                if (src2->value.u[k].u == 0)
                {
                    hlsl_error(ctx, loc, VKD3D_SHADER_ERROR_HLSL_DIVISION_BY_ZERO, "Division by zero.");
                    return false;
                }
                dst->u[k].u = src1->value.u[k].u % src2->value.u[k].u;
                break;

            default:
                FIXME("Fold modulus for type %s.\n", debug_hlsl_type(ctx, dst_type));
                return false;
        }
    }
    return true;
}

static bool fold_max(struct hlsl_ctx *ctx, struct hlsl_constant_value *dst, const struct hlsl_type *dst_type,
        const struct hlsl_ir_constant *src1, const struct hlsl_ir_constant *src2)
{
    enum hlsl_base_type type = dst_type->base_type;
    unsigned int k;

    assert(type == src1->node.data_type->base_type);
    assert(type == src2->node.data_type->base_type);

    for (k = 0; k < dst_type->dimx; ++k)
    {
        switch (type)
        {
            case HLSL_TYPE_INT:
                dst->u[k].i = max(src1->value.u[k].i, src2->value.u[k].i);
                break;

            case HLSL_TYPE_UINT:
                dst->u[k].u = max(src1->value.u[k].u, src2->value.u[k].u);
                break;

            default:
                FIXME("Fold max for type %s.\n", debug_hlsl_type(ctx, dst_type));
                return false;
        }
    }
    return true;
}

static bool fold_min(struct hlsl_ctx *ctx, struct hlsl_constant_value *dst, const struct hlsl_type *dst_type,
        const struct hlsl_ir_constant *src1, const struct hlsl_ir_constant *src2)
{
    enum hlsl_base_type type = dst_type->base_type;
    unsigned int k;

    assert(type == src1->node.data_type->base_type);
    assert(type == src2->node.data_type->base_type);

    for (k = 0; k < dst_type->dimx; ++k)
    {
        switch (type)
        {
            case HLSL_TYPE_INT:
                dst->u[k].i = min(src1->value.u[k].i, src2->value.u[k].i);
                break;

            case HLSL_TYPE_UINT:
                dst->u[k].u = min(src1->value.u[k].u, src2->value.u[k].u);
                break;

            default:
                FIXME("Fold min for type %s.\n", debug_hlsl_type(ctx, dst_type));
                return false;
        }
    }
    return true;
}

static bool fold_bit_xor(struct hlsl_ctx *ctx, struct hlsl_ir_constant *dst,
        struct hlsl_ir_constant *src1, struct hlsl_ir_constant *src2)
{
    enum hlsl_base_type type = dst->node.data_type->base_type;
    unsigned int k;

    assert(type == src1->node.data_type->base_type);
    assert(type == src2->node.data_type->base_type);

    for (k = 0; k < dst->node.data_type->dimx; ++k)
    {
        switch (type)
        {
            case HLSL_TYPE_INT:
            case HLSL_TYPE_UINT:
                dst->value.u[k].u = src1->value.u[k].u ^ src2->value.u[k].u;
                break;

            default:
                FIXME("Fold bit xor for type %s.\n", debug_hlsl_type(ctx, dst->node.data_type));
                return false;
        }
    }
    return true;
}

static bool fold_bit_and(struct hlsl_ctx *ctx, struct hlsl_ir_constant *dst,
        struct hlsl_ir_constant *src1, struct hlsl_ir_constant *src2)
{
    enum hlsl_base_type type = dst->node.data_type->base_type;
    unsigned int k;

    assert(type == src1->node.data_type->base_type);
    assert(type == src2->node.data_type->base_type);

    for (k = 0; k < dst->node.data_type->dimx; ++k)
    {
        switch (type)
        {
            case HLSL_TYPE_INT:
            case HLSL_TYPE_UINT:
                dst->value.u[k].u = src1->value.u[k].u & src2->value.u[k].u;
                break;

            default:
                FIXME("Fold bit and for type %s.\n", debug_hlsl_type(ctx, dst->node.data_type));
                return false;
        }
    }
    return true;
}

static bool fold_bit_or(struct hlsl_ctx *ctx, struct hlsl_ir_constant *dst,
        struct hlsl_ir_constant *src1, struct hlsl_ir_constant *src2)
{
    enum hlsl_base_type type = dst->node.data_type->base_type;
    unsigned int k;

    assert(type == src1->node.data_type->base_type);
    assert(type == src2->node.data_type->base_type);

    for (k = 0; k < dst->node.data_type->dimx; ++k)
    {
        switch (type)
        {
            case HLSL_TYPE_INT:
            case HLSL_TYPE_UINT:
                dst->value.u[k].u = src1->value.u[k].u | src2->value.u[k].u;
                break;

            default:
                FIXME("Fold bit or for type %s.\n", debug_hlsl_type(ctx, dst->node.data_type));
                return false;
        }
    }
    return true;
}

bool hlsl_fold_constant_exprs(struct hlsl_ctx *ctx, struct hlsl_ir_node *instr, void *context)
{
    struct hlsl_ir_constant *arg1, *arg2 = NULL, *res;
    struct hlsl_ir_expr *expr;
    unsigned int i;
    bool success;

    if (instr->type != HLSL_IR_EXPR)
        return false;
    expr = hlsl_ir_expr(instr);
    if (!expr->operands[0].node)
        return false;

    if (instr->data_type->class > HLSL_CLASS_VECTOR)
        return false;

    for (i = 0; i < ARRAY_SIZE(expr->operands); ++i)
    {
        if (expr->operands[i].node)
        {
            if (expr->operands[i].node->type != HLSL_IR_CONSTANT)
                return false;
            assert(expr->operands[i].node->data_type->class <= HLSL_CLASS_VECTOR);
        }
    }
    arg1 = hlsl_ir_constant(expr->operands[0].node);
    if (expr->operands[1].node)
        arg2 = hlsl_ir_constant(expr->operands[1].node);

    if (!(res = hlsl_new_constant(ctx, instr->data_type, &instr->loc)))
        return false;

    switch (expr->op)
    {
        case HLSL_OP1_ABS:
            success = fold_abs(ctx, &res->value, instr->data_type, arg1);
            break;

        case HLSL_OP1_CAST:
            success = fold_cast(ctx, &res->value, instr->data_type, arg1);
            break;

        case HLSL_OP1_NEG:
            success = fold_neg(ctx, &res->value, instr->data_type, arg1);
            break;

        case HLSL_OP2_ADD:
            success = fold_add(ctx, &res->value, instr->data_type, arg1, arg2);
            break;

        case HLSL_OP2_MUL:
            success = fold_mul(ctx, &res->value, instr->data_type, arg1, arg2);
            break;

        case HLSL_OP2_NEQUAL:
            success = fold_nequal(ctx, &res->value, instr->data_type, arg1, arg2);
            break;

        case HLSL_OP2_DIV:
            success = fold_div(ctx, &res->value, instr->data_type, arg1, arg2, &instr->loc);
            break;

        case HLSL_OP2_MOD:
            success = fold_mod(ctx, &res->value, instr->data_type, arg1, arg2, &instr->loc);
            break;

        case HLSL_OP2_MAX:
            success = fold_max(ctx, &res->value, instr->data_type, arg1, arg2);
            break;

        case HLSL_OP2_MIN:
            success = fold_min(ctx, &res->value, instr->data_type, arg1, arg2);
            break;

        case HLSL_OP2_BIT_XOR:
            success = fold_bit_xor(ctx, res, arg1, arg2);
            break;

        case HLSL_OP2_BIT_AND:
            success = fold_bit_and(ctx, res, arg1, arg2);
            break;

        case HLSL_OP2_BIT_OR:
            success = fold_bit_or(ctx, res, arg1, arg2);
            break;

        default:
            FIXME("Fold \"%s\" expression.\n", debug_hlsl_expr_op(expr->op));
            success = false;
            break;
    }

    if (success)
    {
        list_add_before(&expr->node.entry, &res->node.entry);
        hlsl_replace_node(&expr->node, &res->node);
    }
    else
    {
        vkd3d_free(res);
    }
    return success;
}

bool hlsl_fold_constant_swizzles(struct hlsl_ctx *ctx, struct hlsl_ir_node *instr, void *context)
{
    struct hlsl_ir_constant *value, *res;
    struct hlsl_ir_swizzle *swizzle;
    unsigned int i;

    if (instr->type != HLSL_IR_SWIZZLE)
        return false;
    swizzle = hlsl_ir_swizzle(instr);
    if (swizzle->val.node->type != HLSL_IR_CONSTANT)
        return false;
    value = hlsl_ir_constant(swizzle->val.node);

    if (!(res = hlsl_new_constant(ctx, instr->data_type, &instr->loc)))
        return false;

    for (i = 0; i < swizzle->node.data_type->dimx; ++i)
        res->value.u[i] = value->value.u[hlsl_swizzle_get_component(swizzle->swizzle, i)];

    list_add_before(&swizzle->node.entry, &res->node.entry);
    hlsl_replace_node(&swizzle->node, &res->node);
    return true;
}
