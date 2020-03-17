/*
Comparator and hashing functions
*/

#include <stdio.h>
#include <math.h>
#include "datam.h"
#include "putter.h"
#include "putops.h"

int32_t userlist_cmp(datam_darr *llst, datam_darr *rlst){
	uservar *lvar, *rvar;
	size_t ln = llst->n, rn = rlst->n;
	size_t li = 0, ri = 0;
	while(li < ln || ri < rn){
		lvar = NULL;
		rvar = NULL;
		if(li < ln)
			datam_darr_get(llst, &lvar, li);
		if(ri < rn)
			datam_darr_get(rlst, &rvar, ri);
		if(lvar == NULL && rvar == NULL){
			li++;
			ri++;
			continue;
		}
		else if(rvar == NULL){
			if(lvar->flags & FLAG_KEY)
				return 1;
			else{
				li++;
				ri++;
				continue;
			}
		}
		else if(lvar == NULL){
			if(rvar->flags & FLAG_KEY)
				return -1;
			else{
				li++;
				ri++;
				continue;
			}
		}
		else{
			if(!(lvar->flags & FLAG_KEY)){
				li++;
				continue;
			}
			if(!(rvar->flags & FLAG_KEY)){
				ri++;
				continue;
			}
			return uservar_cmp(lvar, rvar);
		}
	}
	return 0;
}

int32_t uservar_cmp_sub(uservar *left, uservar *right){
	var_type com = common_math_type(left->type, right->type);
	switch(com){
		case wstring:
			return wstr_cmp(left->values.list_val->data, right->values.list_val->data);
		case big_fixed:
			put_tobigfix(op1, left);
			put_tobigfix(op2, right);
			return bicmp(op1, op2);
		case big_integer:
			put_tobigint(op1, left);
			put_tobigint(op2, right);
			return bicmp(op1, op2);
		case native_float:{
			float l = put_tofloat(left), r = put_tofloat(right);
			return (l > r) ? 1 : (l < r) ? -1 : 0;
		}
		case code_point:
			return left->values.uni_val - right->values.uni_val;
		case list:{
			datam_darr *llst = left->values.list_val, *rlst = right->values.list_val;
			return userlist_cmp(llst, rlst);
		}
		case block_code:{
			code_block *lcod = left->values.code_val, *rcod = right->values.code_val;
			int32_t bv = userlist_cmp(lcod->bound_vars, rcod->bound_vars);
			if(bv == 0){
				pushable lp, rp;
				for(size_t i = 0; i < lcod->members->n && i < rcod->members->n; i++){
					datam_darr_get(lcod->members, &lp, i);
					datam_darr_get(rcod->members, &rp, i);
					if(lp.type != rp.type)
						return lp.type - rp.type;
					switch(lp.type){
						case literal:{
							uservar *lvar = lp.values.lit_val, *rvar = rp.values.lit_val;
							int32_t vv = uservar_cmp(lvar, rvar);
							if(vv != 0)
								return vv;
						}
						case builtin:
							return (int32_t)(lp.values.op_val) - (int32_t)(rp.values.op_val);
						case varname:
							return wstr_cmp(lp.values.name_val, rp.values.name_val);
					}
				}
				return (int32_t)(lcod->members->n) - (int32_t)(rcod->members->n);
			}
		}
	}
	return 0;
}

int32_t uservar_cmp(void *vleft, void *vright){
	uservar *left = vleft, *right = vright;
	if((left->flags & FLAG_RECUR) || (right->flags & FLAG_RECUR))
		return 0;
	left->flags |= FLAG_RECUR;
	right->flags |= FLAG_RECUR;
	int32_t dif = uservar_cmp_sub(left, right);
	left->flags &= ~FLAG_RECUR;
	right->flags &= ~FLAG_RECUR;
	return dif;
}

int32_t uservar_hsh_sub(uservar *var, size_t n){
	int32_t hash = var->type;
	switch(var->type){
		case big_fixed:
		case big_integer:{
			for(int i = 0; i < bigint_size; i += n + 1){
				hash += var->values.big_val->value->digits[i];
			}
			return hash;
		}
		case native_float:
			return hash + var->values.float_val + n * (int32_t)(log(var->values.float_val));
		case code_point:
			return hash + var->values.uni_val >> n;
		case wstring:
			return hash + wstr_ind(var->values.list_val->data, n);
		case list:{
			datam_darr *lst = var->values.list_val;
			uservar *child;
			for(size_t i = 0; i < lst->n;){
				do{
					datam_darr_get(lst, &child, i++);
				}while(!(child->flags & FLAG_HASH) && i < lst->n);
				if(i < lst->n)
					hash += uservar_hsh_sub(child, n);
				for(size_t j = 0; j < n + 1 && i < lst->n; j++){
					do{
						datam_darr_get(lst, &child, i++);
					}while(!(child->flags & FLAG_HASH) && i < lst->n);
				}
			}
			return hash;
		}
		case block_code:{
			datam_darr *mems = var->values.code_val->members;
			pushable p;
			for(size_t i = 0; i < mems->n; i += n + 1){
				datam_darr_get(mems, &p, i);
				if(p.type == literal)
					hash += uservar_hsh_sub(p.values.lit_val, n);
			}
			return hash;
		}
	}
	return 0;
}

int32_t uservar_hsh(void *vvar, size_t n){
	uservar *var = (uservar*)vvar;
	if(var->flags & FLAG_RECUR)
		return 0;
	var->flags |= FLAG_RECUR;
	int32_t hash = uservar_hsh_sub(var, n);
	var->flags &= ~FLAG_RECUR;
	return hash;
}