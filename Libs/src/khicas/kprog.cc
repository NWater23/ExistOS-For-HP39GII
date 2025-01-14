/* -*- mode:C++ ; compile-command: "g++ -DHAVE_CONFIG_H -I. -I.. -DIN_GIAC -DGIAC_GENERIC_CONSTANTS  -g -c -fno-strict-aliasing prog.cc -Wall" -*- */
#include "giacPCH.h"

/*
 *  Copyright (C) 2001,14 B. Parisse, Institut Fourier, 38402 St Martin d'Heres
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
using namespace std;
#ifndef HAVE_NO_PWD_H
#ifndef BESTA_OS
#include <pwd.h>
#endif
#endif
#ifdef FXCG
extern "C" {
//#include <fx/libfx.h>


}
#endif
#include <stdexcept>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include "prog.h"
#include "identificateur.h"
#include "symbolic.h"
#include "identificateur.h"
#include "usual.h"
#include "sym2poly.h"
#include "subst.h"
#include "plot.h"
#include "tex.h"
#include "input_parser.h"
#include "input_lexer.h"
#include "rpn.h"
#include "help.h"
#include "ti89.h" // for _unarchive_ti
#include "permu.h"
#include "modpoly.h"
#include "unary.h"
#include "input_lexer.h"
#include "maple.h"
#include "derive.h"
#include "giacintl.h"
#include "misc.h"
#include "lin.h"
#include "pari.h"
#include "intg.h"
#include "csturm.h"
#include "sparse.h"
#include "giacintl.h"
// #include "input_parser.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_LIBDL
#include <dlfcn.h>
#endif // HAVE_LIBDL
#ifdef USE_GMP_REPLACEMENTS
#undef HAVE_GMPXX_H
#undef HAVE_LIBMPFR
#undef HAVE_LIBPARI
#endif

#include "syscalls.h"
//#ifdef BESTA_OS
//unsigned int PrimeGetNow(); 
//#endif

#ifdef RTOS_THREADX 
u32 PrimeGetNow();
extern "C" uint32_t mainThreadStack[];
#else
#if !defined BESTA_OS && !defined FXCG
#include <time.h>
#endif
#endif


#ifndef NO_NAMESPACE_GIAC
namespace giac {
#endif // ndef NO_NAMESPACE_GIAC

#if 1 // defined(GIAC_HAS_STO_38) && defined(VISUALC)
  const int rand_max2=2147483647;
#else
  const int rand_max2=RAND_MAX;
#endif
  const double inv_rand_max2_p1=1.0/(rand_max2+1.0);

#ifdef HAVE_LIBDL
  modules_tab giac_modules_tab;
#endif

  void alert(const string & s,GIAC_CONTEXT){
#ifdef EMCC
    EM_ASM_ARGS({
	if (UI.warnpy){
          var msg = Pointer_stringify($0); // Convert message to JS string
          alert(msg);                      // Use JS version of alert          
        }
      }, s.c_str());
#endif
    *logptr(contextptr) << "// " << s << endl;
  }

  gen equaltosto(const gen & g,GIAC_CONTEXT){
    if (g.type<=_IDNT || !eval_equaltosto(contextptr))
      return g;
    if (g.is_symb_of_sommet(at_add_autosimplify)){
      return symbolic(g._SYMBptr->sommet,equaltosto(g._SYMBptr->feuille,contextptr));
    }
    if (is_equal(g)){
      vecteur v=*g._SYMBptr->feuille._VECTptr;
      gen a;
      if (v.size()==2)
	a=v.back();
      else
	a=gen(vecteur(v.begin()+1,v.end()),g.subtype);
      if (v.front().type==_IDNT)
	return symb_sto(a,v.front());
      if (v.front().type==_VECT){
	vecteur w=*v.front()._VECTptr;
	for (int i=0;i<w.size();++i){
	  if (w[i].type!=_IDNT)
	    return g;
	}
	return symb_sto(a,v.front());
      }
    }
    return g;
  }

  vecteur equaltostov(const vecteur & v,GIAC_CONTEXT){
    vecteur w(v);
    iterateur it=w.begin(),itend=w.end();
    for (;it!=itend;++it)
      *it=equaltosto(*it,contextptr);
    return w;
  }

  gen attoof(const gen & g){
    if (g.type==_VECT){
      vecteur v=*g._VECTptr;
      iterateur it=v.begin(),itend=v.end();
      for (;it!=itend;++it)
       *it=attoof(*it);
      return gen(v,g.subtype);
    }
    if (g.type!=_SYMB)
      return g;
    if (g._SYMBptr->sommet!=at_at)
      return symbolic(g._SYMBptr->sommet,attoof(g._SYMBptr->feuille));
    if (g._SYMBptr->feuille.type!=_VECT || g._SYMBptr->feuille._VECTptr->size()<=1) // not somehting that I recognize as a proper parameter list, just return as before
      return symbolic(at_of,attoof(g._SYMBptr->feuille));
    // This looks like it is a vector of at least 2 objects. The first one should be the variable to do an at/of on and all the rest should be indices that needs to be incremented
    vecteur v=*g._SYMBptr->feuille._VECTptr; 
    iterateur it=v.begin()+1,itend=v.end(); // from item 1 to n-1 in the vector
    for (;it!=itend;++it) 
      *it=attoof(*it)+gen(1); // add 1 to the object
    return symbolic(at_of,attoof(gen(v,g._SYMBptr->feuille.subtype))); // return the of(vector with modified indices) gen
  }

  static int prog_eval_level(GIAC_CONTEXT){
    if (int i=prog_eval_level_val(contextptr))
      return i;
    return std::max(1,eval_level(contextptr));
  }

  string indent(GIAC_CONTEXT){
    return " \n"+string(debug_ptr(contextptr)->indent_spaces,' ');
  }

  static string indent2(GIAC_CONTEXT){
    return string(debug_ptr(contextptr)->indent_spaces,' ');
  }

  // static gen substsametoequal(const gen & g){  return symbolic(at_subst,apply(g,sametoequal)); }

  // static gen subssametoequal(const gen & g){  return symbolic(at_subs,apply(g,sametoequal));  }

  // static gen maplesubssametoequal(const gen & g){  return symbolic(at_maple_subs,apply(g,sametoequal)); }

  gen equaltosame(const gen & a){
    // full replacement of = by == has been commented to avoid
    // problems with tests like: if (limit(...,x=0,..))
    /*
    unary_function_ptr equaltosametab1[]={at_equal,at_subst,at_subs,at_maple_subs};
    vector<unary_function_ptr> substin(equaltosametab1,equaltosametab1+sizeof(equaltosametab1)/sizeof(unary_function_ptr));
    gen_op equaltosametab2[]={symb_same,substsametoequal,subssametoequal,maplesubssametoequal};
    vector<gen_op> substout(equaltosametab2,equaltosametab2+sizeof(equaltosametab2)/sizeof(gen_op));
    gen tmp=subst(a,substin,substout,true);
    return tmp;
    */
    if ( is_equal(a))
      return symb_same(a._SYMBptr->feuille._VECTptr->front(),a._SYMBptr->feuille._VECTptr->back());
    else
      return a;
  }

  gen sametoequal(const gen & a){
    if ( (a.type==_SYMB) && (a._SYMBptr->sommet==at_same) )
      return symb_equal(a._SYMBptr->feuille._VECTptr->front(),a._SYMBptr->feuille._VECTptr->back());
    else
      return a;
  }

  static void increment_instruction(const const_iterateur & it0,const const_iterateur & itend,debug_struct * dbgptr){
    const_iterateur it=it0;
    for (;it!=itend;++it)
      increment_instruction(*it,dbgptr);
  }

  void increment_instruction(const vecteur & v,debug_struct * dbgptr){
    const_iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it)
      increment_instruction(*it,dbgptr);
  }

  void increment_instruction(const gen & arg,debug_struct * dbgptr){
    // cerr << debug_ptr(contextptr)->current_instruction << " " << arg <<endl;
    ++dbgptr->current_instruction;
    if (arg.type!=_SYMB)
      return;
    const unary_function_ptr & u=arg._SYMBptr->sommet;
    const gen & f=arg._SYMBptr->feuille;
    const unary_function_eval * uptr=dynamic_cast<const unary_function_eval *>(u.ptr());
    if (uptr && uptr->op==_ifte){
      --dbgptr->current_instruction;
      increment_instruction(*f._VECTptr,dbgptr);
      return;
    }
    if ( (u==at_local) || (uptr && uptr->op==_for) ){
      gen F(f._VECTptr->back());
      if (F.type!=_VECT){
	if (F.is_symb_of_sommet(at_bloc) && F._SYMBptr->feuille.type==_VECT)
	  increment_instruction(*F._SYMBptr->feuille._VECTptr,dbgptr);
	else
	  increment_instruction(F,dbgptr);
      }
      else 
	increment_instruction(*F._VECTptr,dbgptr);
      return;
    }
    if (u==at_bloc){
      if (f.type!=_VECT)
	increment_instruction(f,dbgptr);
      else
	increment_instruction(*f._VECTptr,dbgptr);
      return;
    }
    if (u==at_try_catch){
      increment_instruction(f._VECTptr->front(),dbgptr);
      increment_instruction(f._VECTptr->back(),dbgptr);
    }
  }

  void increment_instruction(const gen & arg,GIAC_CONTEXT){
    increment_instruction(arg,debug_ptr(contextptr));
  }

  static string concatenate(const vector<string> & v){
    vector<string>::const_iterator it=v.begin(),itend=v.end();
    string res;
    for (;it!=itend;++it){
      res=res + "  "+*it;
    }
    return res;
  }

  void debug_print(const vecteur & arg,vector<string> & v,GIAC_CONTEXT){
    const_iterateur it=arg.begin(),itend=arg.end();
    for (;it!=itend;++it){
      if (it->is_symb_of_sommet(at_program)){
	vector<string> tmp;
	debug_print(*it,tmp,contextptr);
	v.push_back(concatenate(tmp));
      }
      debug_print(*it,v,contextptr);
    }
  }

  static string printaslocalvars(const gen &loc,GIAC_CONTEXT){
    gen locals(loc);
    if (locals._VECTptr->size()==1)
      return locals._VECTptr->front().print(contextptr);
    locals.subtype=_SEQ__VECT;
    return locals.print(contextptr);
  }

  // convert back increment and decrement to sto
  static gen from_increment(const gen & g){
    int type=0;
    if (g.is_symb_of_sommet(at_increment))
      type=1;
    if (g.is_symb_of_sommet(at_decrement))
      type=-1;
    if (type){
      gen & f =g._SYMBptr->feuille;
      if (f.type!=_VECT)
	return symbolic(at_sto,makesequence(symb_plus(f,type),f));
      vecteur & v = *f._VECTptr;
      if (v.size()!=2)
	return gensizeerr(gettext("from_increment"));
      return symbolic(at_sto,makesequence(symb_plus(v[0],type*v[1]),v[0]));
    }
    return g;
  }

  void debug_print(const gen & e,vector<string>  & v,GIAC_CONTEXT){
    if (e.type!=_SYMB){
      v.push_back(indent2(contextptr)+e.print(contextptr));
      return ;
    }
    bool python=python_compat(contextptr);
    unary_function_ptr u=e._SYMBptr->sommet;
    gen f=e._SYMBptr->feuille;
    const unary_function_eval * uptr=dynamic_cast<const unary_function_eval *>(u.ptr());
    if (uptr && uptr->op==_ifte){
      string s=indent2(contextptr);
      s += python?"if ":"if(";
      vecteur w=*f._VECTptr;
      s += w.front().print(contextptr);
      s += python?":":")";
      v.push_back(s);
      debug_ptr(contextptr)->indent_spaces += 1;
      debug_print(w[1],v,contextptr);
      debug_ptr(contextptr)->indent_spaces += 1;
      debug_print(w[2],v,contextptr);
      debug_ptr(contextptr)->indent_spaces -=2;
      return ;
    }
    if (u==at_local){
      string s(indent2(contextptr));
      s += python?"# local ":"local ";
      gen local_global=f._VECTptr->front(),locals(gen2vecteur(local_global)),globals(vecteur(0));
      if (local_global.type==_VECT && local_global._VECTptr->size()==2){ 
	gen f=local_global._VECTptr->front(),b=local_global._VECTptr->back();
	if (f.type!=_IDNT){
	  locals=gen2vecteur(f);
	  globals=gen2vecteur(b);
	}
      }
      if (globals._VECTptr->empty())
	s += printaslocalvars(locals,contextptr);
      else
	s += local_global.print(contextptr);
      v.push_back(s);
      debug_ptr(contextptr)->indent_spaces += 2;
      f=f._VECTptr->back();
      if (f.type!=_VECT)
	debug_print(f,v,contextptr);
      else
	debug_print(*f._VECTptr,v,contextptr);
      debug_ptr(contextptr)->indent_spaces -= 2;
      return;
    }
    if (uptr && uptr->op==_for){
      vecteur w=*f._VECTptr;
      string s(indent2(contextptr));
      bool done=false;
      if (python){
	gen inc(from_increment(w[2]));
	if (inc.is_symb_of_sommet(at_sto) && inc._SYMBptr->feuille[1].type==_IDNT){
	  gen index=inc._SYMBptr->feuille[1];
	  if (inc._SYMBptr->feuille[0]==index+1 && w[0].is_symb_of_sommet(at_sto) && w[0]._SYMBptr->feuille[1]==index){
	    gen start=w[0]._SYMBptr->feuille[0];
	    if (w[1].type==_SYMB && (w[1]._SYMBptr->sommet==at_inferieur_egal || w[1]._SYMBptr->sommet==at_inferieur_strict) && w[1]._SYMBptr->feuille[0]==index){
	      int large=w[1]._SYMBptr->sommet==at_inferieur_egal?1:0;
	      gen stop=w[1]._SYMBptr->feuille[1];
	      s += "for "+index.print(contextptr)+" in range(";
	      if (start!=0)
		s +=start.print(contextptr)+",";
	      s += (stop+large).print(contextptr)+"):";
	      done=true;
	    }
	  }
	}
      }
      if (w[0].type==_INT_ && w[2].type==_INT_){
	s += python?"while ":"while(";
	s += w[1].print(contextptr);
	s += python?":":")";
	done=true;
      }
      if (!done){
	s += "for(";
	s += w[0].print(contextptr)+";"+w[1].print(contextptr)+";"+w[2].print(contextptr)+")";
      }
      v.push_back(s);
      debug_ptr(contextptr)->indent_spaces += 2;
      f=f._VECTptr->back();
      if ((f.type==_SYMB) && (f._SYMBptr->sommet==at_bloc))
	f=f._SYMBptr->feuille;
      if (f.type!=_VECT)
	debug_print(f,v,contextptr);
      else
	debug_print(*f._VECTptr,v,contextptr);
      debug_ptr(contextptr)->indent_spaces -= 2;
      return;
    }
    if (u==at_bloc){
      v.push_back(string(indent2(contextptr)+"bloc"));
      debug_ptr(contextptr)->indent_spaces += 2;
      if (f.type!=_VECT)
	debug_print(f,v,contextptr);
      else
	debug_print(*f._VECTptr,v,contextptr);
      debug_ptr(contextptr)->indent_spaces -= 2;
      return;
    }
    if (u==at_try_catch){
      // cerr << f << endl;
      v.push_back(string(indent2(contextptr)+"try"));
      debug_ptr(contextptr)->indent_spaces += 1;
      debug_print(f._VECTptr->front(),v,contextptr);
      debug_ptr(contextptr)->indent_spaces += 1;
      debug_print(f._VECTptr->back(),v,contextptr);
      debug_ptr(contextptr)->indent_spaces -=2;
      return;
    }
    v.push_back(indent2(contextptr)+e.print(contextptr));
  }

  static vecteur rm_checktype(const vecteur & v,GIAC_CONTEXT){
    vecteur addvars(v);
    iterateur it=addvars.begin(),itend=addvars.end();
    for (;it!=itend;++it){
      if (it->is_symb_of_sommet(at_deuxpoints))
	*it=it->_SYMBptr->feuille._VECTptr->front();	
      if (it->is_symb_of_sommet(at_check_type))
	*it=it->_SYMBptr->feuille._VECTptr->back();
      if (it->is_symb_of_sommet(at_sto))
	*it=it->_SYMBptr->feuille._VECTptr->back();	
      if (it->is_symb_of_sommet(at_equal))
	*it=it->_SYMBptr->feuille._VECTptr->front();	
    }
    return addvars;
  }
  // res1= list of assignation with =, res2= list of non declared global vars, res3= list of declared global vars, res4=list of functions
  void check_local_assign(const gen & g,const vecteur & prog_args,vecteur & res1,vecteur & res2,vecteur & res3,vecteur & res4,bool testequal,GIAC_CONTEXT){
    if (g.is_symb_of_sommet(at_double_deux_points))
      return;
    if (g.is_symb_of_sommet(at_local)){
      gen &f=g._SYMBptr->feuille;
      if (f.type!=_VECT || f._VECTptr->size()!=2)
	return;
      gen declaredvars=f._VECTptr->front();
      if (declaredvars.type==_VECT && declaredvars._VECTptr->size()==2){
	vecteur f1(gen2vecteur(declaredvars._VECTptr->front()));
	for (int i=0;i<int(f1.size());++i){
	  if (f1[i].is_symb_of_sommet(at_equal))
	    f1[i]=f1[i]._SYMBptr->feuille[0];
	}
	vecteur f2(gen2vecteur(declaredvars._VECTptr->back()));
	res3=mergevecteur(res3,f2);
	declaredvars=mergevecteur(f1,f2);
      }
      vecteur addvars(rm_checktype(gen2vecteur(declaredvars),contextptr));
      vecteur newvars(mergevecteur(prog_args,addvars));
      // check_local_assign(f._VECTptr->front(),newvars,res1,res2,res3,res4,testequal,contextptr);
      check_local_assign(f._VECTptr->back(),newvars,res1,res2,res3,res4,testequal,contextptr);
      return; 
    }
    if (g.is_symb_of_sommet(at_sto)){
      gen &f=g._SYMBptr->feuille;
      if (f.type==_VECT && f._VECTptr->size()==2 && f._VECTptr->front().is_symb_of_sommet(at_program)){
	res4.push_back(f._VECTptr->back());
	gen & ff = f._VECTptr->front()._SYMBptr->feuille;
	if (ff.type==_VECT && ff._VECTptr->size()==3){
	  vecteur alt_prog_args(gen2vecteur(ff._VECTptr->front()));
	  check_local_assign(ff._VECTptr->back(),alt_prog_args,res1,res2,res3,res4,testequal,contextptr);
	}
	return;
      }
    }
    if (g.is_symb_of_sommet(at_ifte) || g.is_symb_of_sommet(at_when)){
      vecteur v=lop(g,at_array_sto);
      if (!v.empty() && logptr(contextptr))
	*logptr(contextptr) << gettext("Warning, =< is in-place assign, check ") << v << endl;
    }
    if (g.is_symb_of_sommet(at_bloc) || 
	g.is_symb_of_sommet(at_for) ||
	g.is_symb_of_sommet(at_ifte) || g.is_symb_of_sommet(at_when)){
      check_local_assign(g._SYMBptr->feuille,prog_args,res1,res2,res3,res4,testequal,contextptr);
      return;
    }
    if (testequal && is_equal(g)){
      if (g._SYMBptr->feuille.type==_VECT && g._SYMBptr->feuille._VECTptr->size()==2 && g._SYMBptr->feuille._VECTptr->front().type!=_INT_ )
	res1.push_back(g);
      return;
    }
    if (g.is_symb_of_sommet(at_of)){
      gen & f=g._SYMBptr->feuille;
      if (f.type!=_VECT || f._VECTptr->size()!=2)
	return;
      if (python_compat(contextptr) || protecteval(f._VECTptr->front(),1,contextptr)!=f._VECTptr->front())
	check_local_assign(f._VECTptr->back(),prog_args,res1,res2,res3,res4,false,contextptr);
      else
	check_local_assign(f,prog_args,res1,res2,res3,res4,false,contextptr);
      return;
    }
    if (g.type==_SYMB){
      check_local_assign(g._SYMBptr->feuille,prog_args,res1,res2,res3,res4,false,contextptr);
      return;
    }
    if (g.type!=_VECT){
      vecteur l(gen2vecteur(_lname(g,contextptr)));
      const_iterateur it=l.begin(),itend=l.end();
      for (;it!=itend;++it){
	if (!equalposcomp(res2,*it) && !equalposcomp(prog_args,*it))
	  res2.push_back(*it);
      }
      return;
    }
    const_iterateur it=g._VECTptr->begin(),itend=g._VECTptr->end();
    for (;it!=itend;++it){
      check_local_assign(*it,prog_args,res1,res2,res3,res4,testequal,contextptr);
    }
  }
  bool is_constant_idnt(const gen & g){
    return g==cst_pi || g==cst_euler_gamma || is_inf(g) || is_undef(g) || (g.type==_IDNT && (strcmp(g._IDNTptr->id_name,"i")==0 || strcmp(g._IDNTptr->id_name,"None")==0 || strcmp(g._IDNTptr->id_name,"cmath")==0 || strcmp(g._IDNTptr->id_name,"math")==0 || strcmp(g._IDNTptr->id_name,"kandinsky")==0 || strcmp(g._IDNTptr->id_name,"pass")==0));
  }

  bool warn_equal_in_prog=true;
  gen _warn_equal_in_prog(const gen & g,GIAC_CONTEXT){
    if (is_zero(g) && g.type!=_VECT){
      warn_equal_in_prog=false;
      return string2gen(gettext("Warning disabled"),false);
    }
    if (is_one(g)){
      warn_equal_in_prog=true;
      return string2gen(gettext("Warning enabled"),false);
    }
    return warn_equal_in_prog;
  }
  static const char _warn_equal_in_prog_s []="warn_equal_in_prog";
  static define_unary_function_eval (__warn_equal_in_prog,&_warn_equal_in_prog,_warn_equal_in_prog_s);
  define_unary_function_ptr5( at_warn_equal_in_prog ,alias_at_warn_equal_in_prog,&__warn_equal_in_prog,0,true);

  // Return the names of variables that are not local in g
  // and the equality that are not used (warning = instead of := )
  string check_local_assign(const gen & g,GIAC_CONTEXT){
    string res;
    if (g.type==_VECT){
      const_iterateur it=g._VECTptr->begin(),itend=g._VECTptr->end();
      for (;it!=itend;++it)
	res += check_local_assign(*it,contextptr);
      return res;
    }
    if (g.is_symb_of_sommet(at_nodisp))
      return check_local_assign(g._SYMBptr->feuille,contextptr);
    if (g.is_symb_of_sommet(at_sto)){
      gen & f =g._SYMBptr->feuille;
      if (f.type!=_VECT || f._VECTptr->size()!=2)
	return res;
      res=check_local_assign(f._VECTptr->front(),contextptr);
      return res.substr(0,res.size()-1)+"\n//"+gettext(" compiling ")+f._VECTptr->back().print(contextptr)+'\n';
    }
    if (!g.is_symb_of_sommet(at_program))
      return res;
    gen & f=g._SYMBptr->feuille;
    if (f.type!=_VECT || f._VECTptr->size()!=3)
      return gettext("// Invalid program");
    vecteur & v =*f._VECTptr;
    vecteur vars=gen2vecteur(v[0]),res1,res2,res3,res4;
    // add implicit declaration of global var in argument optional value
    for (int i=0;i<vars.size();i++){
      if (vars[i].is_symb_of_sommet(at_equal)){
	gen g=vars[i]._SYMBptr->feuille[1];
	res2=mergevecteur(res2,lidnt(g));
      }
    }
    res2.push_back(undef);
    vars=rm_checktype(vars,contextptr);
    //vecteur vars=rm_checktype(gen2vecteur(v[0]),contextptr),res1,res2(1,undef),res3,res4;
    for (unsigned i=0;i<vars.size();++i){
      if (equalposcomp(vars,vars[i])!=int(i+1))
	res += gettext("// Warning, duplicate argument name: ")+vars[i].print(contextptr)+'\n';
    }
    gen prog=v.back();
    check_local_assign(prog,vars,res1,res2,res3,res4,true,contextptr);
    int rs=int(res2.size());
    for (int i=0;i<rs;i++){
      if (is_constant_idnt(res2[i])){
	res2.erase(res2.begin()+i);
	--i; --rs;
      }
    }
    if (warn_equal_in_prog && !res1.empty()){ // syntax = for := is now accepted
      res += gettext("// Warning, assignation is :=, check the lines below:\n");
      res += "// (Run warn_equal_in_prog(0) to disable this warning)\n";
      const_iterateur it=res1.begin(),itend=res1.end();
      for (;it!=itend;++it){
	res += '\n'+it->print(contextptr);
      }
      res +="\n";
    }
    if (res2.size()>=1){
      res+=gettext("// Warning: ");
      const_iterateur it=res2.begin(),itend=res2.end();
      for (;it!=itend;++it){
	// pi already checked if (*it!=cst_pi)
	res += it->print(contextptr)+",";
      }
      res +=gettext(" declared as global variable(s). If symbolic variables are required, declare them as local and run purge\n");
    }
    if (res.empty())
      return first_error_line(contextptr)?gettext("// Error(s)\n"):gettext("// Success\n");
    else
      return res;
  }

  static string printascheck_type(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if ( (feuille.type!=_VECT) || (feuille._VECTptr->size()!=2) )
      return string(sommetstr)+('('+feuille.print(contextptr)+')');
    return print_the_type(feuille._VECTptr->front().val,contextptr)+' '+feuille._VECTptr->back().print(contextptr);
  }
  
  gen symb_check_type(const gen & args,GIAC_CONTEXT){
    return symbolic(at_check_type,args);
  }
  gen _check_type(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symb_check_type(args,contextptr);
    if (args._VECTptr->size()!=2)
      return gensizeerr(gettext("check_type must have 2 args"));
    gen res=args._VECTptr->back();
    gen req=args._VECTptr->front();
    if (req.type!=_INT_) // FIXME check for matrix(...) or vector(...)
      return res;
    int type;
    switch (res.type){
    case _INT_:
      type=_ZINT;
      break;
    default:
      type=res.type;
      break;
    }   
    if (req.val==_MAPLE_LIST){
      if (type==_VECT)
	return res;
      return gensizeerr(contextptr);
    }
    if (type==req.val)
      return res;
    if (type==_ZINT && type==(req.val &0xff) ){
      if (req.val==_POSINT && is_strictly_positive(res,contextptr)) 
	return res;
      if (req.val==_NEGINT && is_strictly_positive(-res,contextptr))
	return res;
      if (req.val==_NONPOSINT && is_positive(-res,contextptr))
	return res;
      if (req.val==_NONNEGINT && is_positive(res,contextptr))
	return res;
    }
    return gentypeerr(gettext("Argument should be of type ")+print_the_type(args._VECTptr->front().val,contextptr));
  }
  static const char _check_type_s []="check_type";
  static define_unary_function_eval2_index (118,__check_type,&symb_check_type,_check_type_s,&printascheck_type);
  define_unary_function_ptr( at_check_type ,alias_at_check_type ,&__check_type);

  // static gen symb_type(const gen & args){  return symbolic(at_type,args); }
  gen _type(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    int type;
    switch (args.type){
    case _INT_:
      type=_ZINT;
      break;
    default:
      if (args.is_symb_of_sommet(at_program))
	type=_FUNC;
      else
	type=args.type;
    }   
    gen tmp(type);
    tmp.subtype=1;
    return tmp;
  }
  static const char _type_s []="type";
  static define_unary_function_eval (__type,&_type,_type_s);
  define_unary_function_ptr5( at_type ,alias_at_type,&__type,0,true);

  gen _nop(const gen & a,GIAC_CONTEXT){
    if ( a.type==_STRNG &&  a.subtype==-1) return  a;
    if (a.type==_VECT && a.subtype==_SEQ__VECT){
      // Workaround so that sequences inside spreadsheet are saved as []
      gen tmp=a;
      tmp.subtype=0;
      return tmp;
    }
    return a;
  }
  static const char _nop_s []="nop";
  static define_unary_function_eval (__nop,&_nop,_nop_s);
  define_unary_function_ptr5( at_nop ,alias_at_nop,&__nop,0,true);

  string printasinnerbloc(const gen & feuille,GIAC_CONTEXT){
    if ( (feuille.type==_SYMB) && feuille._SYMBptr->sommet==at_bloc)
      return printasinnerbloc(feuille._SYMBptr->feuille,contextptr);
    if (feuille.type!=_VECT)
      return indent(contextptr)+feuille.print(contextptr);
    const_iterateur it=feuille._VECTptr->begin(),itend=feuille._VECTptr->end();
    string res;
    if (it==itend)
      return res;
    for (;;){
      res += indent(contextptr)+it->print(contextptr);
      ++it;
      if (it==itend)
	return res;
    }
  }

  static void local_init(const gen & e,vecteur & non_init_vars,vecteur & initialisation_seq){
    vecteur v;
    if (e.type!=_VECT)
      v=vecteur(1,e);
    else
      v=*e._VECTptr;
    if (v.size()==2 && v.front().type==_VECT)
      v=*v.front()._VECTptr;
    const_iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it){
      if (it->type==_IDNT){
	non_init_vars.push_back(*it);
	continue;
      }
      if ( (it->type==_SYMB) && (it->_SYMBptr->sommet==at_sto)){
	non_init_vars.push_back(it->_SYMBptr->feuille._VECTptr->back());
	initialisation_seq.push_back(*it);
      }
    }
  }

  static gen add_global(const gen & i,GIAC_CONTEXT){
#ifndef NO_STDEXCEPT
    if (i.type==_IDNT)
#endif
      return identificateur("global_"+i.print(contextptr));
    return gensizeerr(gettext("Proc Parameters"));
  }

  static string remove_empty_lines(const string & s){
    // return s;
    string res;
    int ss=int(s.size()),ns=0;
    bool blank=true;
    for (int i=0;i<ss;++i){
      char ch=s[i];
      if (!blank){
	res += ch;
	if (ch=='\n'){
	  ns=0;
	  blank=true;
	}
	continue;
      }
      if (ch=='\n'){
	ns=0;
	continue;
      }
      if (ch!=' '){
	blank=false;
	res += string(ns,' ')+ch;
	ns=0;
	continue;
      }
      ++ns;
    }
    return res;
  }

  static string printasprogram(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if ( (feuille.type!=_VECT) || (feuille._VECTptr->size()!=3) )
      return string(sommetstr)+('('+feuille.print(contextptr)+')');
    string res;
    bool python=python_compat(contextptr) && !debug_ptr(contextptr)->debug_mode;
    if (python){
      int & ind=debug_ptr(contextptr)->indent_spaces;
      vecteur & v =*feuille._VECTptr;
      if (!v[2].is_symb_of_sommet(at_bloc) && !v[2].is_symb_of_sommet(at_local)){
	res=string(ind,' ')+"lambda ";
	if (v[0].type==_VECT && v[0].subtype==_SEQ__VECT && v[0]._VECTptr->size()==1)
	  res += v[0]._VECTptr->front().print(contextptr);
	else 
	  res += v[0].print(contextptr);
	res +=':';
	if (v[2].is_symb_of_sommet(at_return))
	  res += v[2]._SYMBptr->feuille.print(contextptr);
	else
	  res += v[2].print(contextptr);
	return res;
      }
      res = string(ind,' ')+"def "+lastprog_name(contextptr)+"(";
      if (v[0].type==_VECT && v[0].subtype==_SEQ__VECT && v[0]._VECTptr->size()==1)
	res += equaltosto(v[0]._VECTptr->front(),contextptr).print(contextptr);
      else
	res += equaltosto(v[0],contextptr).print(contextptr);
      res += "):\n";
      ind += 4;
      if (v[2].is_symb_of_sommet(at_bloc) || v[2].is_symb_of_sommet(at_local))
	res += v[2].print(contextptr);
      else
	res += string(ind,' ')+v[2].print(contextptr)+'\n';
      ind -= 4;
      // remove empty lines in res
      return remove_empty_lines(res);
    }
    res=" "; // was res=indent(contextptr);
    gen & feuille0=feuille._VECTptr->front();
    if (feuille0.type==_VECT && feuille0.subtype==_SEQ__VECT && feuille0._VECTptr->size()==1)
      res +="("+feuille0._VECTptr->front().print(contextptr)+")";
    else
      res +="("+feuille0.print(contextptr)+")";
    res += "->";
    bool test;
    string locals,inits;
    gen proc_args=feuille._VECTptr->front();
    vecteur vect,non_init_vars,initialisation_seq;
    test=(feuille._VECTptr->back().type!=_VECT ||feuille._VECTptr->back().subtype );
    if (!test)
      vect=*feuille._VECTptr->back()._VECTptr;
    if (test){
      gen & fb=feuille._VECTptr->back();
      if (fb.is_symb_of_sommet(at_local)){
	gen fb0=fb._SYMBptr->feuille[0][0];
	if (fb0.type==_VECT && fb0._VECTptr->empty())
	  return res+'{'+fb.print(contextptr)+'}';
      }
      return res+fb.print(contextptr);
    }
    res += "{";
    const_iterateur it=vect.begin(),itend=vect.end();
    debug_ptr(contextptr)->indent_spaces +=2;
    for (;;){
      res += indent(contextptr)+it->print(contextptr);
      ++it;
      if (it==itend){
	debug_ptr(contextptr)->indent_spaces -=2;
	res += "}";
	break;
      }
    }
    return res;
  }

  // utility for default argument handling
  static void default_args(gen & a,gen & b,GIAC_CONTEXT){
#ifndef GIAC_DEFAULT_ARGS
    return;
#endif
    if (a.type==_VECT && b.type==_VECT && a._VECTptr->size()==b._VECTptr->size()){
      vecteur va=*a._VECTptr;
      vecteur vb=*b._VECTptr;
      for (unsigned i=0;i<a._VECTptr->size();++i)
	default_args(va[i],vb[i],contextptr);
      a=gen(va,a.subtype);
      b=gen(vb,b.subtype);
      return;
    }
    if (a.is_symb_of_sommet(at_sto)){
      b=a._SYMBptr->feuille[0];
      a=a._SYMBptr->feuille[1];
      return;
    }
    if (a.is_symb_of_sommet(at_equal)){
      b=a._SYMBptr->feuille[1];
      a=a._SYMBptr->feuille[0];
      return;
    }
    b=string2gen(gettext("Unitialized parameter ")+a.print(contextptr),false);
    b.subtype=-1;
  }

  static void replace_keywords(const gen & a,const gen & b,gen & newa,gen & newb,GIAC_CONTEXT){
    // Check that variables in a are really variables, otherwise print
    // the var and make new variables
    vecteur newv(gen2vecteur(a));
    if (newv.size()==2 && newv.front().type==_VECT && newv.back().type==_VECT){
      gen tmpa,tmpb;
      replace_keywords(*newv.front()._VECTptr,b,tmpa,tmpb,contextptr);
      replace_keywords(*newv.back()._VECTptr,tmpb,newa,newb,contextptr);
      newa=gen(makevecteur(tmpa,newa),a.subtype);
      return;
    }
    vecteur v1,v2;
    iterateur it=newv.begin(),itend=newv.end();
    for (;it!=itend;++it){
      if (it->is_symb_of_sommet(at_sto) || it->is_symb_of_sommet(at_check_type) || it->is_symb_of_sommet(at_equal)) // FIXME check 1st arg too
	continue;
      if (it->is_symb_of_sommet(at_of)){
	*logptr(contextptr) << gettext("Invalid argument name ") << *it << gettext(". You should use ") << it->_SYMBptr->feuille._VECTptr->front() << gettext(" instead, even if the argument should be of type function") << endl;
	*it=it->_SYMBptr->feuille._VECTptr->front();
      }
      if (it->is_symb_of_sommet(at_deuxpoints)){
	gen theid=it->_SYMBptr->feuille[0];
	gen egal=0;
	if (theid.is_symb_of_sommet(at_sto)){
	  egal=theid._SYMBptr->feuille[0];
	  theid=theid._SYMBptr->feuille[1];
	}
	if (theid.is_symb_of_sommet(at_equal)){
	  egal=theid._SYMBptr->feuille[1];
	  theid=theid._SYMBptr->feuille[0];
	}
	gen thetype=it->_SYMBptr->feuille[1],newid=*it;
	if (thetype.type==_INT_ && thetype.subtype==_INT_TYPE){
	  v1.push_back(theid);
	  // add = 
	  if (thetype.val==_ZINT){
	    newid=*it=gen(identificateur(theid.print(contextptr)+"_i"));
	    if (egal!=0)
	      *it=symb_equal(*it,egal);
	  }
	  if (thetype.val==_DOUBLE_ ){
	    newid=*it=gen(identificateur(theid.print(contextptr)+"_d"));
	    if (egal!=0)
	      *it=symb_equal(*it,egal);
	  }
	  if (thetype.val==_CPLX){
	    newid=*it=gen(identificateur(theid.print(contextptr)+"_c"));
	    if (egal!=0)
	      *it=symb_equal(*it,egal);
	  }
	  if (thetype.val==_FRAC){
	    newid=*it=gen(identificateur(theid.print(contextptr)+"_f"));
	    if (egal!=0)
	      *it=symb_equal(*it,egal);
	  }
	  v2.push_back(newid);
	  continue;
	}
	if (thetype==at_real || thetype==at_float){
	  v1.push_back(theid);
	  *it=gen(identificateur(theid.print(contextptr)+"_d"));
	  if (egal!=0)
	    *it=symb_equal(*it,egal);
	  v2.push_back(*it);
	  continue;
	}
	if (thetype==at_complex){
	  v1.push_back(theid);
	  *it=gen(identificateur(theid.print(contextptr)+"_c"));
	  if (egal!=0)
	    *it=symb_equal(*it,egal);
	  v2.push_back(*it);
	  continue;
	}
	if (thetype==at_vector){
	  v1.push_back(theid);
	  newid=*it=gen(identificateur(theid.print(contextptr)+"_v"));
	  if (egal!=0)
	    *it=symb_equal(*it,egal);
	  v2.push_back(newid);
	  continue;
	}
	if (thetype==at_string){
	  v1.push_back(theid);
	  newid=*it=gen(identificateur(theid.print(contextptr)+"_s"));
	  if (egal!=0)
	    *it=symb_equal(*it,egal);
	  v2.push_back(newid);
	  continue;
	}
	if (thetype==at_integrate || thetype==at_int){ // int==integrate
	  v1.push_back(theid);
	  newid=*it=gen(identificateur(theid.print(contextptr)+"_i"));
	  if (egal!=0)
	    *it=symb_equal(*it,egal);
	  v2.push_back(newid);
	  continue;
	}
      }
      if (it->type!=_IDNT && it->type!=_CPLX){
	v1.push_back(*it);
	string s=gen2string(*it);
	int ss=int(s.size());
	if (ss==1)
	  s += "___";
	if (ss>2 && s[0]=='\'' && s[ss-1]=='\'')
	  s=s.substr(1,ss-2);
	for (unsigned i=0;i<s.size();++i){
	  if (!isalpha(s[i]))
	    s[i]='_';
	}
	lock_syms_mutex();  
	sym_string_tab::const_iterator i = syms().find(s);
	if (i == syms().end()) {
	  *it = *(new identificateur(s));
	  syms()[s] = *it;
	} else {
	  // std::cerr << "lexer" << s << endl;
	  *it = i->second;
	}
	unlock_syms_mutex();  
	v2.push_back(*it);
      }
    }
    newa=gen(newv,_SEQ__VECT);
    if (v1.empty())
      newb=b;
    else {
      *logptr(contextptr) << gettext("Invalid or typed variable(s) name(s) were replaced by creating special identifiers, check ") << v1 << endl;
      newb=quotesubst(b,v1,v2,contextptr);
    }
  }

  string mkvalid(const string & s){
    string res;
    for (size_t i=0;i<s.size();++i){
      char ch=s[i];
      if (ch!='.' && isalphan(ch)){
	res += ch;
	continue;
      }
      res += "char";
      res += char('0'+(ch/100));
      res += char('0'+((ch%100)/10));
      res += char('0'+(ch%10));
    }
    return res;
  }

  // a=arguments, b=values, c=program bloc, d=program name
  symbolic symb_program_sto(const gen & a_,const gen & b_,const gen & c_,const gen & d,bool embedd,GIAC_CONTEXT){
    gen a(a_),b(b_),c(c_);
    default_args(a,b,contextptr);
    bool warn=false;
#ifndef GIAC_HAS_STO_38
    if (logptr(contextptr) && calc_mode(contextptr)!=1)
      warn=true;
#endif
    if (warn){
      *logptr(contextptr) << gettext("// Parsing ") << d << endl;
      lastprog_name(d.print(contextptr),contextptr);
      if (c.is_symb_of_sommet(at_derive))
	*logptr(contextptr) << gettext("Warning, defining a derivative function should be done with function_diff or unapply: ") << c << endl;
       if (c.type==_SYMB && c._SYMBptr->sommet!=at_local && c._SYMBptr->sommet!=at_bloc && c._SYMBptr->sommet!=at_when && c._SYMBptr->sommet!=at_for && c._SYMBptr->sommet!=at_ifte){
	 vecteur lofc=lop(c,at_of);
	 vecteur lofc_no_d;
	 vecteur av=gen2vecteur(a);
	 for (unsigned i=0;i<lofc.size();++i){
	   if (lofc[i][1]!=d && !equalposcomp(av,lofc[i][1]) )
	     lofc_no_d.push_back(lofc[i]);
	 }
	 if (!lofc_no_d.empty()){
	   *logptr(contextptr) << gettext("Warning: algebraic function defined in term of others functions may lead to evaluation errors") << endl;
	   CERR << c.print(contextptr) << endl;
	   *logptr(contextptr) << gettext("Perhaps you meant ") << d.print(contextptr) << ":=unapply(" << c.print(contextptr) << ",";
	   if (a.type==_VECT && a.subtype==_SEQ__VECT && a._VECTptr->size()==1)
	     *logptr(contextptr) << a._VECTptr->front().print(contextptr) << ")" << endl;
	   else
	     *logptr(contextptr) << a.print(contextptr) << ")" << endl;
	 }
       }
    }
    vecteur newcsto(lop(c,at_sto)),newc1,newc2;
    for (size_t i=0;i<newcsto.size();++i){
      gen var=newcsto[i]._SYMBptr->feuille[1];
      if (var.type==_FUNC && (python_compat(contextptr) || !archive_function_index(*var._FUNCptr))){
	newc1.push_back(var);
	newc2.push_back(identificateur(mkvalid(var._FUNCptr->ptr()->print(contextptr))+"_rep"));
      }
    }
    newcsto=mergevecteur(lop(c,at_struct_dot),lop(c,at_for));
    for (size_t i=0;i<newcsto.size();++i){
      gen & val=newcsto[i]._SYMBptr->feuille._VECTptr->front();
      if (val.type==_VECT && (is_numericv(*val._VECTptr) || is_integer_vecteur(*val._VECTptr))) // in-place modification
	val=symbolic(at_copy,val);
      gen var=newcsto[i]._SYMBptr->feuille._VECTptr->back();
      if (var.type==_FUNC && var!=at_random && (python_compat(contextptr) || !archive_function_index(*var._FUNCptr))){
	newc1.push_back(var);
	newc2.push_back(identificateur(mkvalid(var._FUNCptr->ptr()->print(contextptr))+"_rep"));
      }
    }
    newcsto=gen2vecteur(a);
    for (size_t i=0;i<newcsto.size();++i){
      gen var=newcsto[i];
      if (var.type==_FUNC){
	newc1.push_back(var);
	newc2.push_back(identificateur(mkvalid(var._FUNCptr->ptr()->print(contextptr))+"_rep"));
      }
    }
    if (!newc1.empty()){
      c=subst(c,newc1,newc2,true,contextptr);
      a=subst(a,newc1,newc2,true,contextptr);
    }
    gen newa,newc;
    replace_keywords(a,((embedd&&c.type==_VECT)?makevecteur(c):c),newa,newc,contextptr);
    //cout << c._SYMBptr->sommet << endl;
    bool cloc=newc.is_symb_of_sommet(at_local),glob=false;
    gen clocg;
    if (cloc){
      clocg=newc._SYMBptr->feuille;
      if (clocg.type==_VECT && !clocg._VECTptr->empty()){
	clocg=clocg._VECTptr->front();
	glob=clocg.type==_VECT && clocg._VECTptr->size()==2 && clocg._VECTptr->front().type==_VECT && clocg._VECTptr->front()._VECTptr->empty();
      }
    }
    if (python_compat(contextptr) && (!cloc || glob) ){
      vecteur res1,non_decl,res3,res4,Newa=gen2vecteur(newa);
      for (int i=0;i<int(Newa.size());++i){
	if (Newa[i].is_symb_of_sommet(at_equal))
	  Newa[i]=Newa[i]._SYMBptr->feuille[0];
      }
      check_local_assign(newc,Newa,res1,non_decl,res3,res4,false,contextptr);
      vecteur stov(lop(newc,at_sto));
      for (size_t i=0;i<stov.size();++i){
	stov[i]=stov[i]._SYMBptr->feuille[1];
      }
      vecteur stoprog(lop(newc,at_program));
      for (size_t i=0;i<stoprog.size();++i){
	stoprog[i]=stoprog[i]._SYMBptr->feuille[0];
      }
      vecteur stofor(lop(newc,at_for));
      for (size_t i=0;i<stofor.size();++i){
	stofor[i]=stofor[i]._SYMBptr->feuille[0];
      }
      stov=lidnt(mergevecteur(mergevecteur(stov,stoprog),stofor));
      int rs=int(non_decl.size());
      for (int i=0;i<rs;i++){
	// remove var that are not assigned (assumed global), constant idnt and recursive def
	gen noni=non_decl[i];
	if (noni.type!=_IDNT){
	  non_decl.erase(non_decl.begin()+i);
	  --i; --rs;
	  continue;
	}
	bool b=equalposcomp(stov,noni);
	if (strcmp(noni._IDNTptr->id_name,"i")==0){
	  if (!b){
	    non_decl.erase(non_decl.begin()+i);
	    --i; --rs;
	  }
	}
	else {
	  if (!b || is_constant_idnt(noni) || noni==d){ 
	    non_decl.erase(non_decl.begin()+i);
	    --i; --rs;
	  }
	}
      }
      if (!non_decl.empty()){
	*logptr(contextptr) << gettext("// Auto-declared local variables : ") << gen(non_decl,_SEQ__VECT) << endl;
	if (glob)
	  newc=symb_local(makesequence(non_decl,clocg._VECTptr->back()),newc._SYMBptr->feuille._VECTptr->back(),contextptr);
	else
	  newc=symb_local(non_decl,newc,contextptr);
      }
    }
    symbolic g=symbolic(at_program,makesequence(newa,b,newc));
    g=symbolic(at_sto,makesequence(g,d));
    if (warn)
      *logptr(contextptr) << check_local_assign(g,contextptr) ;
    if (warn && newc.is_symb_of_sommet(at_local)){
      // check that a local variable name does not shadow a parameter name
      gen & newcf=newc._SYMBptr->feuille;
      if (newcf.type==_VECT && newcf._VECTptr->size()==2){
	gen & vars = newcf._VECTptr->front();
	gen inters=_intersect(makesequence(vars,newa),contextptr);
	if (inters.type==_VECT && !inters._VECTptr->empty()){
	  inters.subtype=_SEQ__VECT;
	  *logptr(contextptr) << gettext("Warning: Local variables shadow function arguments ") << inters << endl;
	}
      }
    }
    if (printprog){
      int p=python_compat(contextptr);
      python_compat(printprog/256,contextptr);
      if (g.sommet==at_sto){
	lastprog_name(g.feuille[1].print(contextptr),contextptr);
	COUT << g.feuille[0].print(contextptr) <<endl;	
      }
      else
	COUT << g <<endl;
      python_compat(p,contextptr);
    }
    return g;
  }
  gen symb_prog3(const gen &a,const gen &b,const gen &c){
    return symbolic(at_program,makesequence(a,b,c));
  }
  symbolic symb_program(const gen & a_,const gen & b_,const gen & c,GIAC_CONTEXT){
    gen a(a_),b(b_);
    default_args(a,b,contextptr);
    gen newa,newc;
    replace_keywords(a,c,newa,newc,contextptr);
    symbolic g=symbolic(at_program,makesequence(newa,b,newc));
#ifndef GIAC_HAS_STO_38
    if (logptr(contextptr))
      *logptr(contextptr) << check_local_assign(g,contextptr) ;
#endif
    return g;
  }
  symbolic symb_program(const gen & args){
    return symbolic(at_program,args);
  }
  static void lidnt_prog(const gen & g,vecteur & res);
  static void lidnt_prog(const vecteur & v,vecteur & res){
    const_iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it)
      lidnt_prog(*it,res);
  }
  static void lidnt_prog(const gen & g,vecteur & res){
    switch (g.type){
    case _VECT:
      lidnt_prog(*g._VECTptr,res);
      break;
    case _IDNT:
      if (!equalposcomp(res,g))
	res.push_back(g);
      break;
    case _SYMB:
      /* if (g._SYMBptr->sommet==at_at || g._SYMBptr->sommet==at_of )
	lidnt_prog(g._SYMBptr->feuille._VECTptr->back(),res);
	else */
	lidnt_prog(g._SYMBptr->feuille,res);
      break;
    }
  }

  static void local_vars(const gen & g,vecteur & v,GIAC_CONTEXT){
    if (g.type==_VECT){
      const_iterateur it=g._VECTptr->begin(),itend=g._VECTptr->end();
      for (;it!=itend;++it){
	local_vars(*it,v,contextptr);
      }
      return ;
    }
    if (g.type!=_SYMB)
      return;
    if (g._SYMBptr->sommet==at_local && g._SYMBptr->feuille.type==_VECT){
      vecteur & w = *g._SYMBptr->feuille._VECTptr;
      if (w[0].type==_VECT && w[0]._VECTptr->size()==2 && w[0]._VECTptr->front().type==_VECT)
	v=mergevecteur(v,*w[0]._VECTptr->front()._VECTptr);
      else
	v=mergevecteur(v,gen2vecteur(w[0]));
      local_vars(w[1],v,contextptr);
      return;
    }
    if (g._SYMBptr->sommet==at_program && g._SYMBptr->feuille.type==_VECT){
      vecteur & w = *g._SYMBptr->feuille._VECTptr;
      if (w[0].type==_VECT )
	v=mergevecteur(v,*w[0]._VECTptr);
    }
    local_vars(g._SYMBptr->feuille,v,contextptr);
  }

  gen quote_program(const gen & args,GIAC_CONTEXT){
    // return symb_program(args);
    // g:=unapply(p ->translation(w,p),w);g(1)
    // Necessite d'evaluer les arguments des programmes a l'interieur d'un programme.
    // Mais il ne faut pas evaluer les variables declarees comme locales!!
    bool in_prog=debug_ptr(contextptr)->sst_at_stack.size()!=0;
    // ?? Subst all variables except arguments
    if (!in_prog || args.type!=_VECT || args._VECTptr->size()!=3)
      return symb_program(args);
    vecteur & v = *args._VECTptr;
    vecteur vars(gen2vecteur(v[0]));
    int s=int(vars.size()); // s vars not subst-ed
    lidnt_prog(v[2],vars);
    vars=vecteur(vars.begin()+s,vars.end());
    // Remove local variables from the list
    vecteur tmpvar,resvar;
    local_vars(v[2],tmpvar,contextptr); 
    const_iterateur it=vars.begin(),itend=vars.end();
    for (;it!=itend;++it){
      if (!equalposcomp(tmpvar,*it))
	resvar.push_back(*it);
    }
    gen tmp=gen(resvar).eval(1,contextptr);
    vecteur varsub(*tmp._VECTptr);
    return symbolic(at_program,quotesubst(args,resvar,varsub,contextptr));
  }
  static bool is_return(const gen & g,gen & newres){
    if (g.type==_SYMB && g._SYMBptr->sommet==at_return){
      // gen tmp = g._SYMBptr->feuille; is_return(tmp,newres);
      is_return(g._SYMBptr->feuille,newres);
      return true;
    }
    if (g.type==_STRNG && g.subtype==-1){
      newres=g;
      return true;
    }
    if ( (g.type==_VECT && g.subtype ==_SEQ__VECT && g._VECTptr->size()==1) )
      return is_return(g._VECTptr->front(),newres);
    newres=g;
    return false;
  }
  void adjust_sst_at(const gen & name,GIAC_CONTEXT){
    debug_ptr(contextptr)->sst_at.clear();
    const_iterateur it=debug_ptr(contextptr)->debug_breakpoint.begin(),itend=debug_ptr(contextptr)->debug_breakpoint.end();
    for (;it!=itend;++it){
      if (it->_VECTptr->front()==name)
	debug_ptr(contextptr)->sst_at.push_back(it->_VECTptr->back().val);
    }
  }

  void program_leave(const gen & save_debug_info,bool save_sst_mode,debug_struct * dbgptr){
    dbgptr->args_stack.pop_back();
    // *logptr(contextptr) << "Leaving " << args << endl;
    if (!dbgptr->sst_at_stack.empty()){
      dbgptr->sst_at=dbgptr->sst_at_stack.back();
      dbgptr->sst_at_stack.pop_back();
    }
    if (!dbgptr->current_instruction_stack.empty()){
      dbgptr->current_instruction=dbgptr->current_instruction_stack.back();
      dbgptr->current_instruction_stack.pop_back();
    }
    dbgptr->sst_mode=save_sst_mode;
    if (dbgptr->current_instruction_stack.empty())
      dbgptr->debug_mode=false;
    (*dbgptr->debug_info_ptr)=save_debug_info;
    (*dbgptr->fast_debug_info_ptr)=save_debug_info;
  }

  gen _program(const gen & args,const gen & name,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return args.eval(prog_eval_level(contextptr),contextptr);
    // set breakpoints
    debug_struct * dbgptr=debug_ptr(contextptr);
    context * newcontextptr=(context *)contextptr;
    const_iterateur it,itend;
    gen res,newres,label,vars,values,prog,save_debug_info;
    // *logptr(contextptr) << & res << endl;
#ifdef RTOS_THREADX
    if ((void *)&res<= (void *)&mainThreadStack[1024]) { 
      gensizeerr(gettext("Too many recursions"),res);
      return res;
    }
#else
#if !defined(WIN32) && defined HAVE_PTHREAD_H
    if (contextptr){
      // CERR << &slevel << " " << thread_param_ptr(contextptr)->stackaddr << endl;
      if ( ((size_t) &res) < ((size_t) thread_param_ptr(contextptr)->stackaddr)+8192){
	gensizeerr(gettext("Too many recursion levels"),res); 
	return res;
      }
    }
    else {
      if ( int(dbgptr->sst_at_stack.size()) >= MAX_RECURSION_LEVEL+1){
	gensizeerr(gettext("Too many recursions"),res);
	return res;
      }
    }
#else
    if ( int(dbgptr->sst_at_stack.size()) >= MAX_RECURSION_LEVEL+1){
      gensizeerr(gettext("Too many recursions"),res);
      return res;
    }
#endif
#endif
    dbgptr->sst_at_stack.push_back(dbgptr->sst_at);
    dbgptr->sst_at.clear();
    if (name.type==_IDNT)
      adjust_sst_at(name,contextptr);
    dbgptr->current_instruction_stack.push_back(dbgptr->current_instruction);
    dbgptr->current_instruction=0;
    bool save_sst_mode = dbgptr->sst_mode,findlabel,calc_save ;
    int protect=0;
    // *logptr(contextptr) << "Entering prog " << args << " " << dbgptr->sst_in_mode << endl;
    if (dbgptr->sst_in_mode){
      dbgptr->sst_in_mode=false;
      dbgptr->sst_mode=true;
    }
    else
      dbgptr->sst_mode=false;
    // Bind local var
#ifdef TIMEOUT
    control_c();
#endif
    if (ctrl_c || interrupted || args._VECTptr->size()!=3){
      gensizeerr(res,contextptr);
      return res;
    }
    calc_save=calc_mode(contextptr)==38;
    if (calc_save) 
      calc_mode(-38,contextptr);
    vars=args._VECTptr->front();
    values=(*(args._VECTptr))[1];
    prog=args._VECTptr->back();
    save_debug_info=(*dbgptr->debug_info_ptr);
    if (vars.type!=_VECT)
      vars=gen(makevecteur(vars));
    if (values.type!=_VECT || values.subtype!=_SEQ__VECT || (vars._VECTptr->size()==1 && values._VECTptr->size()>1))
      values=gen(makevecteur(values));
    // *logptr(contextptr) << vars << " " << values << endl;
    dbgptr->args_stack.push_back(gen(mergevecteur(vecteur(1,name),*values._VECTptr)));
    // removed sst test so that when a breakpoint is evaled
    // the correct info is displayed
    (*dbgptr->debug_info_ptr)=prog;
    (*dbgptr->fast_debug_info_ptr)=prog;
    if (!vars._VECTptr->empty())
      protect=giac_bind(*values._VECTptr,*vars._VECTptr,newcontextptr);
    if (protect==-RAND_MAX){
      program_leave(save_debug_info,save_sst_mode,dbgptr);
      if (calc_save) 
	calc_mode(38,contextptr);
      gensizeerr(res,contextptr);
      return res;
    }
#ifndef NO_STDEXCEPT
    try {
#endif
      if (prog.type!=_VECT || prog.subtype){
	++debug_ptr(newcontextptr)->current_instruction;
	prog=equaltosto(prog,contextptr);
	if (debug_ptr(newcontextptr)->debug_mode){
	  debug_loop(res,newcontextptr);
	  if (!is_undef(res)){
	    if (!prog.in_eval(prog_eval_level(newcontextptr),res,newcontextptr))
	      res=prog;
	  }
	}
	else {
	  if (!prog.in_eval(prog_eval_level(newcontextptr),res,newcontextptr))
	    res=prog;
	}
	if (is_return(res,newres))
	  res=newres;
      }
      else {
	it=prog._VECTptr->begin();
	itend=prog._VECTptr->end();
	findlabel=false;
	for (;!ctrl_c && !interrupted && it!=itend;++it){
#ifdef TIMEOUT
	  control_c();
#endif
	  ++debug_ptr(newcontextptr)->current_instruction;
	  if (debug_ptr(newcontextptr)->debug_mode){
	    debug_loop(res,newcontextptr);
	    if (is_undef(res)) break;
	  }
	  if (!findlabel){
	    if (it->is_symb_of_sommet(at_return)){
	      if (!equaltosto(it->_SYMBptr->feuille,contextptr).in_eval(prog_eval_level(newcontextptr),newres,newcontextptr))
		newres=it->_SYMBptr->feuille;
	      is_return(newres,res);
	      break;
	    }
	    if (!equaltosto(*it,contextptr).in_eval(prog_eval_level(newcontextptr),res,newcontextptr))
	      res=*it;
	  }
	  else
	    res=*it;
	  if (res.type==_STRNG && res.subtype==-1)
	    break;
	  if (res.type==_SYMB){
	    if (findlabel && res.is_symb_of_sommet(at_label) && label==res._SYMBptr->feuille)
	      findlabel=false;
	    if (!findlabel && res.is_symb_of_sommet(at_goto)){
	      findlabel=true;
	      label=res._SYMBptr->feuille;
	    }
	  }
	  if (findlabel && it+1==itend)
	    it=prog._VECTptr->begin()-1;
	  if (!findlabel && is_return(res,newres)){
	    res=newres;
	    break;
	  }
	}
      }
#ifndef NO_STDEXCEPT
    } // end try
    catch (std::runtime_error & e){
      last_evaled_argptr(contextptr)=NULL;
      if (!vars._VECTptr->empty())
	leave(protect,*vars._VECTptr,newcontextptr);
      if (calc_save) 
	calc_mode(38,contextptr);
      throw(std::runtime_error(e.what()));
    }
#endif
    if (!vars._VECTptr->empty())
      leave(protect,*vars._VECTptr,newcontextptr);
    program_leave(save_debug_info,save_sst_mode,dbgptr);
    if (calc_save) 
      calc_mode(38,contextptr);
    return res;
  }
  static const char _program_s []="program";
  static define_unary_function_eval4_index (147,__program,&quote_program,_program_s,&printasprogram,&texprintsommetasoperator);
  define_unary_function_ptr5( at_program ,alias_at_program,&__program,_QUOTE_ARGUMENTS,0);

  gen symb_return(const gen & arg){
    return gen(symbolic(at_return,arg));
  }
  static string printasbloc(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if ( feuille.type!=_VECT || feuille._VECTptr->empty() )
      return "{"+feuille.print(contextptr)+";}";
    const_iterateur it=feuille._VECTptr->begin(),itend=feuille._VECTptr->end();
    string res("{");
    bool python=python_compat(contextptr) && !debug_ptr(contextptr)->debug_mode;
    if (python){
      int & ind=debug_ptr(contextptr)->indent_spaces;
      res="\n";
      for (;it!=itend;++it){
	string add=it->print(contextptr);
	if (add.size() && add[0]=='\n' && res.size() && res[res.size()-1]=='\n')
	  res += add.substr(1,add.size()-1)+'\n';
	else
	  res += string(ind,' ')+add+'\n';
      }
      return res;
    }
    debug_ptr(contextptr)->indent_spaces +=2;
    for (;;){
	res += indent(contextptr)+it->print(contextptr);
      ++it;
      if (it==itend){
	debug_ptr(contextptr)->indent_spaces -=2;
	res += "; "+indent(contextptr);
	res += "}";
	break;
      }
      else {
	res +="; ";
      }
    }
    return res;
  }
  gen symb_bloc(const gen & args){
    if (args.type!=_VECT)
      return args;
    if (args.type==_VECT && args._VECTptr->size()==1)
      return args._VECTptr->front();
    gen a=args; a.subtype=_SEQ__VECT;
    return symbolic(at_bloc,a);
  }
  gen _bloc(const gen & prog,GIAC_CONTEXT){
    if ( prog.type==_STRNG &&  prog.subtype==-1) return  prog;
    gen res,label;
    bool findlabel=false;
    debug_struct * dbgptr=debug_ptr(contextptr);
    if (prog.type!=_VECT){
      ++dbgptr->current_instruction;
      if (dbgptr->debug_mode){
	debug_loop(res,contextptr);
	if (is_undef(res)) return res;
      }
      return prog.eval(eval_level(contextptr),contextptr);
    }
    else {
      const_iterateur it=prog._VECTptr->begin(),itend=prog._VECTptr->end();
      for (;!ctrl_c && !interrupted && it!=itend;++it){
#ifdef TIMEOUT
	control_c();
#endif
	++dbgptr->current_instruction;
	if (dbgptr->debug_mode){
	  debug_loop(res,contextptr);
	  if (is_undef(res)) return res;
	}
	if (!findlabel){
	  if (it->type==_SYMB && it->_SYMBptr->sommet==at_return){
	    // res=it->_SYMBptr->feuille.eval(prog_eval_level(contextptr),contextptr);
	    if (!it->_SYMBptr->feuille.in_eval(prog_eval_level(contextptr),res,contextptr))
	      res=it->_SYMBptr->feuille;
	    increment_instruction(it+1,itend,dbgptr);
	    return symb_return(res);
	  }
	  else {
	    // res=it->eval(eval_level(contextptr),contextptr);
	    if (!equaltosto(*it,contextptr).in_eval(eval_level(contextptr),res,contextptr))
	      res=*it;
	  }
	}
	else 
	  res=*it;
	if (res.type==_STRNG && res.subtype==-1)
	  return res;
	if (res.type==_SYMB){
	  unary_function_ptr & u=res._SYMBptr->sommet;
	  if (!findlabel && (u==at_return || u==at_break)) {
	    increment_instruction(it+1,itend,dbgptr);
	    return res; // it->eval(eval_level(contextptr),contextptr);
	  }
	  if (!findlabel && u==at_goto){
	    findlabel=true;
	    label=res._SYMBptr->feuille;
	  }
	  if ( u==at_label && label==res._SYMBptr->feuille )
	    findlabel=false;
	}
	// restart the bloc if needed
	if (findlabel && it+1==itend)
	  it=prog._VECTptr->begin()-1;
      }
    }
    return res;
  }
  static const char _bloc_s []="bloc";
  static define_unary_function_eval2_index (145,__bloc,&_bloc,_bloc_s,&printasbloc);
  define_unary_function_ptr5( at_bloc ,alias_at_bloc,&__bloc,_QUOTE_ARGUMENTS,0);

  // test
  string printasifte(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if ( (feuille.type!=_VECT) || (feuille._VECTptr->size()!=3) )
      return string(sommetstr)+('('+feuille.print(contextptr)+')');
    const_iterateur it=feuille._VECTptr->begin();//,itend=feuille._VECTptr->end();
    string res="if ";
    bool python=python_compat(contextptr) && !debug_ptr(contextptr)->debug_mode;
    if (python){
      int & ind=debug_ptr(contextptr)->indent_spaces;
      res = '\n'+string(ind,' ')+string("if ")+it->print(contextptr)+" :\n";
      ind += 4;
      ++it;
      res += string(ind,' ')+it->print(contextptr);
      ++it;
      if (it->type!=_INT_)
	res += '\n'+string(ind-4,' ')+string("else :\n")+string(ind,' ')+it->print(contextptr);
      ind -= 4;
      return res;
    }
    res += "("+it->print(contextptr);
    ++it;
    res += ") ";
    debug_ptr(contextptr)->indent_spaces +=2;
    res += it->print(contextptr);
    char reslast=res[res.size()-1];
    if (reslast!=';' && reslast!='}') 
      res += ";";
    debug_ptr(contextptr)->indent_spaces -=2;
    ++it;
    if (!is_zero(*it)){
      if (res[res.size()-1]=='}')
	res += indent(contextptr)+" else ";
      else {
	reslast=res[res.size()-1];
	if (reslast!=';' && reslast!='}') 
	  res += ";";
	res+= " else ";
      }
      debug_ptr(contextptr)->indent_spaces +=2;
      res += it->print(contextptr);
      if (res[res.size()-1]!=';' 
	  //&& !xcas_mode(contextptr)
	  )
	res += ";";
      debug_ptr(contextptr)->indent_spaces -=2;
    }
    // FIXME? NO ; AT END OF IF
    if (res[res.size()-1]!='}')
      res +=" ";
    return res;
  }
  symbolic symb_ifte(const gen & test,const gen & oui, const gen & non){    return symbolic(at_ifte,makesequence(test,oui,non));  }

  gen symb_when(const gen & arg){
    return gen(symbolic(at_when,arg));
  }

  gen ifte(const gen & args,bool isifte,const context * contextptr){
    gen test,res;
    if (args.type!=_VECT || args._VECTptr->size()!=3){
      gensizeerr(gettext("Ifte must have 3 args"),res);
      return res;
    }
    int evallevel=eval_level(contextptr);
#if 1
    res=args._VECTptr->front();
    if (!res.in_eval(evallevel,test,contextptr))
      test=res;
    if (test.type!=_INT_){
      test=equaltosame(test).eval(evallevel,contextptr);
      if (!is_integer(test)){
	test=test.type==_MAP?!test._MAPptr->empty():test.evalf_double(evallevel,contextptr);
	if (test.type==_VECT && python_compat(contextptr)){
	  // OR test on a list
	  const_iterateur it=test._VECTptr->begin(),itend=test._VECTptr->end();
	  for (;it!=itend;++it){
	    if (!is_zero(*it,contextptr)){
	      test=*it;
	      break;
	    }
	  }
	  if (it==itend)
	    test=0;
	}
	if ( test.type>_CPLX ){
	  if (isifte){
	    gensizeerr(gettext("Ifte: Unable to check test"),res); 
	    return res;
	  }
	  else
	    return symb_when(eval(args,1,contextptr));
	}
      }
    }
#else
    test=args._VECTptr->front();
    test=equaltosame(test.eval(eval_level(contextptr),contextptr)).eval(eval_level(contextptr),contextptr);
    if (!is_integer(test)){
      test=test.evalf_double(eval_level(contextptr),contextptr);
      if ( (test.type!=_DOUBLE_) && (test.type!=_CPLX) ){
	if (isifte){
	  gensizeerr(gettext("Ifte: Unable to check test"),res); 
	  return res;
	}
	else
	  return symb_when(eval(args,1,contextptr));
      }
    }
#endif
    bool rt;
    vecteur *argsptr=args._VECTptr;
    // *logptr(contextptr) << "Ifte " << debug_ptr(contextptr)->current_instruction << endl ;
    if (is_zero(test)){ // test false, do the else part
      if (isifte){
	debug_struct * dbgptr=debug_ptr(contextptr);
	increment_instruction((*argsptr)[1],dbgptr);
	// *logptr(contextptr) << "Else " << debug_ptr(contextptr)->current_instruction << endl ;
	++dbgptr->current_instruction;
	if (dbgptr->debug_mode){
	  debug_loop(test,contextptr);
	  if (is_undef(test)) return test;
	}
      }
      if (argsptr->back().type==_INT_)
	return argsptr->back();
      gen clause_fausse=equaltosto(argsptr->back(),contextptr);
      rt=clause_fausse.is_symb_of_sommet(at_return);
      if (rt)
	clause_fausse=clause_fausse._SYMBptr->feuille;
      // res=clause_fausse.eval(evallevel,contextptr);
      if (!clause_fausse.in_eval(evallevel,res,contextptr))
	res=clause_fausse;
      if (rt && (res.type!=_SYMB || res._SYMBptr->sommet!=at_return))
	res=symb_return(res);
      // *logptr(contextptr) << "Else " << debug_ptr(contextptr)->current_instruction << endl ;
    }
    else { // test true, do the then part
      if (isifte){
	debug_struct * dbgptr=debug_ptr(contextptr);
	++dbgptr->current_instruction;
	if (dbgptr->debug_mode){
	  debug_loop(test,contextptr);
	  if (is_undef(test)) return test;
	}
      }
      gen clause_vraie=equaltosto((*argsptr)[1],contextptr);
      rt=clause_vraie.is_symb_of_sommet(at_return);
      if (rt)
	clause_vraie=clause_vraie._SYMBptr->feuille;
      // res=clause_vraie.eval(evallevel,contextptr);
      if (!clause_vraie.in_eval(evallevel,res,contextptr))
	res=clause_vraie;
      if (rt && (res.type!=_SYMB || res._SYMBptr->sommet!=at_return) )
	res=symb_return(res);
      // *logptr(contextptr) << "Then " << debug_ptr(contextptr)->current_instruction << endl ;
      if (isifte)
	increment_instruction(argsptr->back(),contextptr);
      // *logptr(contextptr) << "Then " << debug_ptr(contextptr)->current_instruction << endl ;
    }
    return res;
  }
  gen _ifte(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return ifte(args,true,contextptr);
  }
  static const char _ifte_s []="ifte";
  static define_unary_function_eval2_index (141,__ifte,&_ifte,_ifte_s,&printasifte);
  define_unary_function_ptr5( at_ifte ,alias_at_ifte,&__ifte,_QUOTE_ARGUMENTS,0);

  gen _evalb(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_VECT) return apply(args,_evalb,contextptr);
    gen test=equaltosame(args);
    test=normal(test,contextptr);
    test=test.eval(eval_level(contextptr),contextptr);
    test=test.evalf_double(1,contextptr);
    if ( (test.type!=_DOUBLE_) && (test.type!=_CPLX) ){
      test=args.eval(eval_level(contextptr),contextptr);
      test=equaltosame(test);
      test=normal(test,contextptr);
      test=test.eval(eval_level(contextptr),contextptr);
      test=test.evalf_double(1,contextptr);
      if ( (test.type!=_DOUBLE_) && (test.type!=_CPLX) ){
	return symbolic(at_evalb,args);
      }
    }
    gen g=is_zero(test)?zero:plus_one;
    g.subtype=_INT_BOOLEAN;
    return g;
  }
  static const char _evalb_s []="evalb";
  static define_unary_function_eval_quoted (__evalb,&_evalb,_evalb_s);
  define_unary_function_ptr5( at_evalb ,alias_at_evalb,&__evalb,_QUOTE_ARGUMENTS,true);

  static const char _maple_if_s []="if";
  static define_unary_function_eval2_quoted (__maple_if,&_ifte,_maple_if_s,&printasifte);
  define_unary_function_ptr5( at_maple_if ,alias_at_maple_if,&__maple_if,_QUOTE_ARGUMENTS,0);

  static string printaswhen(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if (feuille.type!=_VECT || feuille._VECTptr->size()!=3)
      return sommetstr+("("+feuille.print(contextptr)+")");
    vecteur & v=*feuille._VECTptr;
    if (python_compat(contextptr)){
      string s="when("+v[0].print(contextptr)+","+v[1].print(contextptr)+","+v[2].print(contextptr)+")";
      return s;
    }
    return "(("+v[0].print(contextptr)+")? "+v[1].print(contextptr)+" : "+v[2].print(contextptr)+")";
  }
  gen symb_when(const gen & t,const gen & a,const gen & b){
    return symbolic(at_when,makesequence(t,a,b));
  }
  gen _when(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return gensizeerr(gettext("3 or 4 arguments expected"));
    vecteur & v=*args._VECTptr;
    if (v.size()==3){
      gen res=ifte(args,false,contextptr);
      return res;
    }
    if (v.size()!=4)
      return gentypeerr(contextptr);
    gen res=ifte(vecteur(v.begin(),v.begin()+3),false,contextptr);
    if (res.type==_SYMB && res._SYMBptr->sommet==at_when)
      return v[3];
    return res;
  }
  static const char _when_s []="when";
  static define_unary_function_eval2_quoted (__when,&_when,_when_s,&printaswhen);
  define_unary_function_ptr5( at_when ,alias_at_when,&__when,_QUOTE_ARGUMENTS,true);

  // loop
  string printasfor(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if (feuille.type!=_VECT) 
      return string(sommetstr)+('('+feuille.print(contextptr)+')');
    if (feuille._VECTptr->size()==2)
      return feuille._VECTptr->front().print(contextptr)+ " in "+feuille._VECTptr->back().print(contextptr);
    if (feuille._VECTptr->size()!=4)
      return string(sommetstr)+('('+feuille.print(contextptr)+')');
    const_iterateur it=feuille._VECTptr->begin();//,itend=feuille._VECTptr->end();
    string res;
    gen inc(from_increment(*(it+2)));
    bool python=python_compat(contextptr) && !debug_ptr(contextptr)->debug_mode;
    if (python){
      int & ind=debug_ptr(contextptr)->indent_spaces;
      if (inc.is_symb_of_sommet(at_sto) && inc._SYMBptr->feuille[1].type==_IDNT){
	gen index=inc._SYMBptr->feuille[1];
	if (inc._SYMBptr->feuille[0]==index+1 && it->is_symb_of_sommet(at_sto) && it->_SYMBptr->feuille[1]==index){
	  gen start=it->_SYMBptr->feuille[0];
	  if ((it+1)->type==_SYMB && ((it+1)->_SYMBptr->sommet==at_inferieur_egal || (it+1)->_SYMBptr->sommet==at_inferieur_strict) && (it+1)->_SYMBptr->feuille[0]==index){
	    int large=(it+1)->_SYMBptr->sommet==at_inferieur_egal?1:0;
	    gen stop=(it+1)->_SYMBptr->feuille[1];
	    res +=  '\n'+string(ind,' ')+string("for ")+index.print(contextptr)+" in range(";
	    if (start!=0)
	      res +=start.print(contextptr)+",";
	    res +=(stop+large).print(contextptr)+"):"+'\n';
	    ind += 4;
	    res += string(ind,' ')+(it+3)->print(contextptr);
	    ind -=4;
	    return res;
	  }
	}
      }
      if (it->type!=_INT_) res += '\n'+string(ind,' ')+it->print(contextptr)+'\n';
      res += '\n'+string(ind,' ')+string("while ") + (it+1)->print(contextptr)+" :";
      if (!(it+3)->is_symb_of_sommet(at_bloc))
	res += '\n';
      ind += 4;
      res += string(ind,' ')+(it+3)->print(contextptr);
      if ((it+2)->type!=_INT_) res += '\n'+string(ind,' ')+(it+2)->print(contextptr);      
      ind -=4;
      return res;
    }
    if (is_integer(*it) && is_integer(*(it+2))){
      ++it;
      if (is_one(*it) && (it+2)->is_symb_of_sommet(at_bloc)){
	const gen & loopf=(it+2)->_SYMBptr->feuille;
	if (loopf.type==_VECT && loopf._VECTptr->back().is_symb_of_sommet(at_ifte)){
	  const vecteur & condv=*loopf._VECTptr->back()._SYMBptr->feuille._VECTptr;
	  if (condv.size()==3 && is_zero(condv[2]) && condv[1].is_symb_of_sommet(at_break)){ 
	    // repeat until condv[0] loop, loopf except the end is the loop
	    vecteur corps=vecteur(loopf._VECTptr->begin(),loopf._VECTptr->end()-1);
	    res = "repeat ";
	    res += printasinnerbloc(corps,contextptr);
	    res += indent(contextptr);
	    res += "until ";
	    res += condv[0].print();
	    return res;
	  }
	}
      }
      bool dodone=false;
      if (it->is_symb_of_sommet(at_for) && it->_SYMBptr->feuille.type==_VECT && it->_SYMBptr->feuille._VECTptr->size()==2){
	res = "for "+it->_SYMBptr->feuille._VECTptr->front().print(contextptr)+" in "+ it->_SYMBptr->feuille._VECTptr->back().print(contextptr)+ " do ";
	dodone=true;
      }
      else {
	res = "while("+it->print(contextptr)+")";
      }
      ++it;
      ++it;
      debug_ptr(contextptr)->indent_spaces += 2;
      if (dodone && it->type==_SYMB && it->_SYMBptr->sommet==at_bloc)
	res += printasinnerbloc(it->_SYMBptr->feuille,contextptr)+";";
      else
	res += it->print(contextptr) +";";
      if (dodone)
	res += " od;";
      debug_ptr(contextptr)->indent_spaces -= 2;
    }
    else {  
      res="for (";
      res += it->print(contextptr) + ';';
      ++it;
      res += it->print(contextptr) + ';';
      ++it;
      res += it->print(contextptr) + ") ";
      ++it;
      debug_ptr(contextptr)->indent_spaces += 2;
      res += it->print(contextptr) ;
      debug_ptr(contextptr)->indent_spaces -= 2;
    }
    if (res[res.size()-1]!='}')
      res += "; ";
    return res;
  }
  //symbolic symb_for(const gen & e){    return symbolic(at_for,e);  }
  //symbolic symb_for(const gen & a,const gen & b,const gen & c,const gen & d){    return symbolic(at_for,makesequence(a,b,c,d));  }
  
  static gen to_increment(const gen & g){
    if (!g.is_symb_of_sommet(at_sto))
      return g;
    gen & f =g._SYMBptr->feuille;
    if (f.type!=_VECT || f._VECTptr->size()!=2)
      return g;
    gen & a = f._VECTptr->front();
    gen & b = f._VECTptr->back();
    if (b.type!=_IDNT || a.type!=_SYMB)
      return g;
    gen & af=a._SYMBptr->feuille;
    if (af.type!=_VECT || af._VECTptr->empty())
      return g;
    vecteur & av= *af._VECTptr;
    int s=int(av.size());
    int type=0;
    if (a.is_symb_of_sommet(at_plus))
      type=1;
    // there was a wrong test with at_minus for -= (type=-1)
    if (type && av.front()==b){
      if (s==2){
	if (is_one(av.back()))
	  return symbolic(type==1?at_increment:at_decrement,b);
	if (is_minus_one(av.back()))
	  return symbolic(type==1?at_decrement:at_increment,b);
	return symbolic(type==1?at_increment:at_decrement,makesequence(b,av.back()));
      }
      if (type)
	return symbolic(at_increment,makesequence(b,symb_plus(vecteur(av.begin()+1,av.end()))));
    }
    return g;
  }
  static bool ck_is_one(gen & g,GIAC_CONTEXT){
    if (is_one(g))
      return true;
    if (g.type==_VECT && python_compat(contextptr)){
      // OR test on a list
      const_iterateur it=g._VECTptr->begin(),itend=g._VECTptr->end();
      for (;it!=itend;++it){
	if (!is_zero(*it,contextptr))
	  return true;
      }
      return false;
    }
    if (g.type>_POLY){
      g=gensizeerr(gettext("Unable to eval test in loop : ")+g.print());
      return false; // this will stop the loop in caller
    }
    return false;
  }
  static bool set_for_in(int counter,int for_in,vecteur * v,string * s,const gen & name,GIAC_CONTEXT){
    int taille=int(v?v->size():s->size());
    if (counter>0 && counter<=taille){
      if (v)
	(*v)[counter-1]=eval(name,1,contextptr);
      else {
	if (s){	 
	  gen g=eval(name,1,contextptr);
	  if (g.type==_STRNG && !g._STRNGptr->empty())
	    (*s)[counter-1]=(*g._STRNGptr)[0];
	}
      }
    }
    if (counter<0 || counter>=taille)
      return false;
    gen res;
    if (v){
      res=(*v)[counter];
      res=sto(res,name,contextptr);
    }
    else {
      char ch=(*s)[counter];
      res=sto(string2gen(string(1,ch),false),name,contextptr);
    }
    return !is_undef(res);
  }
  inline bool for_test(const gen & test,gen & testf,int eval_lev,context * contextptr){
    // ck_is_one( (testf=test.eval(eval_lev,newcontextptr).evalf(1,newcontextptr)) )
    if (!test.in_eval(eval_lev,testf,contextptr)) 
      testf=test;
    //testf=test.eval(eval_lev,contextptr);
    if (testf.type==_INT_) 
      return testf.val;
    if (testf.type==_FRAC) 
      testf=testf._FRACptr->num;
    if (test.type<=_POLY)
      return !is_exactly_zero(test);
    if (testf.type==_MAP)
      return !testf._MAPptr->empty();
    testf=testf.evalf(1,contextptr);
    return ck_is_one(testf,contextptr);
  }

  // return false if forprog modifies index or stopg
  bool chk_forprog(const gen & forprog,const gen & index,const gen & stopg);
  bool chk_forprog(const vecteur & forprog,const gen & index,const gen & stopg){
    const_iterateur it=forprog.begin(),itend=forprog.end();
    for (;it!=itend;++it){
      if (!chk_forprog(*it,index,stopg))
	return false;
    }
    return true;
  }
  bool chk_forprog(const gen & forprog,const gen & index,const gen & stopg){
    if (forprog.type==_VECT)
      return chk_forprog(*forprog._VECTptr,index,stopg);
    if (forprog.type!=_SYMB || forprog.subtype==_FORCHK__SYMB)
      return true;
    unary_function_ptr & u=forprog._SYMBptr->sommet;
    if (u==at_sto || u==at_array_sto){
      const gen * to=&(*forprog._SYMBptr->feuille._VECTptr)[1];
      if (*to==index || *to==stopg)
	return false;
    }
    if (u==at_increment || u==at_decrement){
      const gen * to=&forprog._SYMBptr->feuille;
      if (to->type==_VECT) to=&to->_VECTptr->front();
      if (*to==index || *to==stopg) 
	return false;
    }
    if (!chk_forprog(forprog._SYMBptr->feuille,index,stopg))
      return false;
    ((gen *) &forprog)->subtype=_FORCHK__SYMB;
    return true;
  }
  gen _for(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    // for elem in list: for(elem,list), inert form
    if (args.type!=_VECT || args._VECTptr->size()==2)
      return symbolic(at_for,args); //symb_for(args);
    const vecteur & argsv = *args._VECTptr;
    if (argsv.size()!=4)
      return gensizeerr(gettext("For must have 4 args"));
    // Initialization
    gen initialisation=argsv.front(),forelse;
    // add assigned variables to be local
    bool bound=false;
    vecteur loop_var;
    int protect=0;
    context * newcontextptr=(context * ) contextptr;
    if ( (initialisation.type==_SYMB) && (initialisation._SYMBptr->sommet==at_sto)){
      gen variable=initialisation._SYMBptr->feuille._VECTptr->back();
      if (variable.type==_IDNT){
	if (contextptr==context0 && (xcas_mode(contextptr)!=1 && (!variable._IDNTptr->localvalue || variable._IDNTptr->localvalue->empty() || (*variable._IDNTptr->localvalue)[variable._IDNTptr->localvalue->size()-2].val<protection_level-1) ) ){
	  bound=true;
	  loop_var=makevecteur(variable);
	  protect=giac_bind(makevecteur(zero),loop_var,newcontextptr);
	}
      }
      else {
#ifndef NO_STDEXCEPT
	throw(std::runtime_error(gettext("Invalid loop index (hint: i=sqrt(-1)!)")));
#endif
	return undeferr(gettext("Invalid loop index (hint: i=sqrt(-1)!)"));
      }
    }
    gen test=argsv[1];
    if (is_equal(test))
      test = symb_same(test._SYMBptr->feuille._VECTptr->front(),test._SYMBptr->feuille._VECTptr->back());
    // FIXME: eval local variables in test that are not in increment and prog
    gen increment=to_increment(argsv[2]);
    gen prog=argsv[3];
    if (prog.type==_SYMB && prog._SYMBptr->sommet==at_bloc)
      prog=prog._SYMBptr->feuille;
    vecteur forprog=prog.type==_VECT?*prog._VECTptr:vecteur(1,prog);
    iterateur it,itbeg=forprog.begin(),itend=forprog.end();
    for (it=itbeg;it!=itend;++it){
      *it=to_increment(equaltosto(*it,contextptr));
    }
    gen res,oldres;
    // loop
    int eval_lev=eval_level(newcontextptr);
    debug_struct * dbgptr=debug_ptr(newcontextptr);
    int & dbgptr_current_instruction = dbgptr->current_instruction;
    int save_current_instruction=dbgptr_current_instruction;
    gen testf;
#ifndef NO_STDEXCEPT
    try {
#endif
      bool findlabel=false;
      gen label,newres;
      int counter=0;
      int for_in=0; // 1 in list, 2 in string
      vecteur * for_in_v=0;
      string * for_in_s=0;
      gen index_name;
      gen gforin; 
      size_t testsize=0;
      if ((test.is_symb_of_sommet(at_for))&& test._SYMBptr->feuille.type==_VECT && (testsize=test._SYMBptr->feuille._VECTptr->size())>=2){
	gen tmp=eval((*test._SYMBptr->feuille._VECTptr)[1],eval_lev,newcontextptr);
	if (testsize==3)
	  forelse=(*test._SYMBptr->feuille._VECTptr)[2];
	bool ismap=tmp.type==_MAP;
	if (ismap){
	  // copy indices
	  vecteur v;
	  gen_map::iterator it=tmp._MAPptr->begin(),itend=tmp._MAPptr->end();
	  for (;it!=itend;++it){
	    v.push_back(it->first);
	  }
	  tmp=v;
	}
	if (tmp.type==_VECT){
	  // re-store a copy of tmp because we might modify it inplace
	  // (not for maps)
	  gforin=gen(*tmp._VECTptr,tmp.subtype);
	  if (!ismap && test._SYMBptr->feuille._VECTptr->back().type==_IDNT)
	    sto(gforin,test._SYMBptr->feuille._VECTptr->back(),false,contextptr);
	  for_in_v=gforin._VECTptr;
	  for_in=1;
	}
	else {
	  if (tmp.type==_STRNG){
	    // re-store a copy of tmp because we might modify it inplace
	    gforin=string2gen(*tmp._STRNGptr,false);
	    if (test._SYMBptr->feuille._VECTptr->back().type==_IDNT)
	      sto(gforin,test._SYMBptr->feuille._VECTptr->back(),false,contextptr);
	    for_in_s=gforin._STRNGptr;
	    for_in=2;
	  }
	  else
	    return gensizeerr(contextptr);
	}
	index_name=test._SYMBptr->feuille._VECTptr->front();
      }
      // check if we have a standard for loop
      bool stdloop=(itend-it)<5 && contextptr && !for_in && is_inequation(test) && test._SYMBptr->feuille.type==_VECT && test._SYMBptr->feuille._VECTptr->size()==2 && !dbgptr->debug_mode;
      stdloop = stdloop && increment.type==_SYMB && (increment._SYMBptr->sommet==at_increment || increment._SYMBptr->sommet==at_decrement);
      gen index,stopg;
      int *idx=0,step,stop;
      if (stdloop){
	index=increment._SYMBptr->feuille;
	if (index.type==_VECT) index=index._VECTptr->front();
	stdloop=index==test._SYMBptr->feuille._VECTptr->front();
      }
      // loop initialisation
      equaltosto(initialisation,contextptr).eval(eval_lev,newcontextptr);
      if (stdloop){
	stopg=test._SYMBptr->feuille._VECTptr->back();
	gen stopindex=eval(stopg,1,contextptr);
	is_integral(stopindex);
	stop=stopindex.val;
	gen incrementstep=increment._SYMBptr->feuille;
	incrementstep=incrementstep.type==_VECT?incrementstep._VECTptr->back():1;
	is_integral(incrementstep);
	step=incrementstep.val;
	if (increment._SYMBptr->sommet==at_decrement) 
	  step=-step;
	stdloop=index.type==_IDNT && incrementstep.type==_INT_ && stopindex.type==_INT_;
      }
      if (stdloop){
	const context * cur=contextptr;
	sym_tab::iterator it=cur->tabptr->end(),itend=it;
	for (;cur->previous;cur=cur->previous){
	  it=cur->tabptr->find(index._IDNTptr->id_name);
	  itend=cur->tabptr->end();
	  if (it!=itend)
	    break;
	}
	if (it==itend){
	  it=cur->tabptr->find(index._IDNTptr->id_name);
	  itend=cur->tabptr->end();
	}
	// compute idx
	if (it!=itend && it->second.type==_INT_ 
     	    && (stop-it->second.val)/step>=5){
	  idx=&it->second.val;
	  // adjust stop for a loop with condition *idx!=stop
	  int niter=-1;
	  unary_function_ptr & u=test._SYMBptr->sommet;
	  if (u==at_inferieur_strict) {
	    if (*idx>=stop)
	      niter=0;
	    else
	      niter=step<0?-1:(stop+step-1-*idx)/step;
	  }
	  if (u==at_superieur_strict){
	    if (*idx<=stop)
	      niter=0;
	    else
	      niter=step>0?-1:(*idx-stop-step-1)/(-step);
	  }
	  if (u==at_inferieur_egal){
	    if (*idx>stop)
	      niter=0;
	    else
	      niter=step<0?-1:(stop+step-*idx)/step;
	  }
	  if (u==at_superieur_egal){
	    if (*idx<stop)
	      niter=0;
	    else
	      niter=step>0?-1:(*idx-stop-step)/(-step);
	  }
	  if (niter<0){
	    if (bound)
	      leave(protect,loop_var,newcontextptr);
	    return gensizeerr("Infinite number of iterations");
	  }
	  stop=*idx+niter*step;
	  // check that index and stopg are not modified inside the loop
	  if (!chk_forprog(forprog,index,stopg))
	    idx=0;
	}
      }
      bool oneiter=idx && (itend-itbeg==1) && !dbgptr->debug_mode;
      if (oneiter){
	for (;!interrupted && *idx!=stop;*idx+=step){
#ifdef SMARTPTR64
	  swapgen(oldres,res);
#else
	  oldres=res;
#endif
	  if (!itbeg->in_eval(eval_lev,res,newcontextptr))
	    res=*itbeg;
	  if (res.type!=_SYMB) 
	    continue;
	  if (is_return(res,newres)) {
	    if (bound)
	      leave(protect,loop_var,newcontextptr);
	    return res;
	  }
	  unary_function_ptr & u=res._SYMBptr->sommet;
	  if (u==at_break){
	    res=u; // res=oldres;
	    break;
	  }
	  if (u==at_continue){
	    res=oldres;
	    break;
	  }
	}
      }
      else {
	for (;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-value"
	     idx?*idx!=stop:((for_in && test.val)?set_for_in(counter,for_in,for_in_v,for_in_s,index_name,newcontextptr):for_test(test,testf,eval_lev,newcontextptr));
	     ++counter,idx?*idx+=step:((test.val && increment.type)?increment.eval(eval_lev,newcontextptr).val:0)
#pragma clang diagnostic pop
	     ){
	  if (interrupted || (testf.type!=_INT_ && is_undef(testf)))
	    break;
	  dbgptr_current_instruction=save_current_instruction;
	  findlabel=false;
	  // add a test for boucle of type program/composite
	  // if that's the case call eval with test for break and continue
	  for (it=itbeg;!interrupted && it!=itend;++it){
#ifdef SMARTPTR64
	    swapgen(oldres,res);
#else
	    oldres=res;
#endif
	    ++dbgptr_current_instruction;
	    if (dbgptr->debug_mode){
	      debug_loop(res,newcontextptr);
	      if (is_undef(res)){
		increment_instruction(it+1,itend,dbgptr);
		if (bound)
		  leave(protect,loop_var,newcontextptr);
		return res;
	      }
	    }
	    if (!findlabel){
	      // res=it->eval(eval_lev,newcontextptr);
	      if (!it->in_eval(eval_lev,res,newcontextptr))
		res=*it;
	      if (res.type<=_POLY) 
		continue;
	    }
	    else {
#ifdef TIMEOUT
	      control_c();
#endif
	      if (ctrl_c || interrupted || (res.type==_STRNG && res.subtype==-1)){
		interrupted = true; ctrl_c=false;
		*logptr(contextptr) << gettext("Stopped in loop") << endl;
		gensizeerr(gettext("Stopped by user interruption."),res);
		break;
	      }
	      res=*it;
	    }
	    if (is_return(res,newres)) {
	      increment_instruction(it+1,itend,dbgptr);
	      if (bound)
		leave(protect,loop_var,newcontextptr);
	      return res;
	    }
	    if (res.type==_SYMB){
	      unary_function_ptr & u=res._SYMBptr->sommet;
	      if (!findlabel){ 
		if (u==at_break){
		  increment_instruction(it+1,itend,dbgptr);
		  test=zero;
		  idx=0;
		  res=u; // res=oldres;
		  break;
		}
		if (u==at_continue){
		  increment_instruction(it+1,itend,dbgptr);
		  res=oldres;
		  break;
		}
	      }
	      else {
		if (u==at_label && label==res._SYMBptr->feuille)
		  findlabel=false;
	      }
	      if (!findlabel && u==at_goto){
		findlabel=true;
		label=res._SYMBptr->feuille;
	      }
	    } // end res.type==_SYMB
	    if (findlabel && it+1==itend)
	      it=itbeg-1;
	  } // end of loop of FOR bloc instructions
	} // end of user FOR loop
      } // end else one iteration
      dbgptr->current_instruction=save_current_instruction;
      increment_instruction(itbeg,itend,dbgptr);
#ifndef NO_STDEXCEPT
    } // end try
    catch (std::runtime_error & e){
      last_evaled_argptr(contextptr)=NULL;
      if (bound)
	leave(protect,loop_var,newcontextptr);
      return gensizeerr(e.what());
      gen res(string2gen(e.what(),false));
      res.subtype=-1;
      return res;
    }
#endif
    if (bound)
      leave(protect,loop_var,newcontextptr);
    if (is_undef(testf))
      return testf;
    if (res!=at_break && forelse!=0)
      forelse.in_eval(eval_lev,res,contextptr);
    return res==at_break?string2gen("breaked",false):res;
  }

  static const char _for_s []="for";
  static define_unary_function_eval2_index (143,__for,&_for,_for_s,&printasfor);
  define_unary_function_ptr5( at_for ,alias_at_for,&__for,_QUOTE_ARGUMENTS,0);

  // returns level or -RAND_MAX on error
  int giac_bind(const vecteur & vals_,const vecteur & vars_,context * & contextptr){
    vecteur vals(vals_),vars(vars_);
#if 1
    int ins=int(vals.size());
    for (int i=int(vars.size())-1;i>=0;--i){
      if (vars.size()<=vals.size())
	break;
      if (vars[i].is_symb_of_sommet(at_equal))
	vals.insert(vals.begin()+ins,eval(vars[i]._SYMBptr->feuille[1],prog_eval_level(contextptr),contextptr));
      else {
	if (ins>0)
	  --ins;
      }
    }
#else
    for (;vars.size()>vals.size();){
      int s=int(vals.size());
      if (!vars[s].is_symb_of_sommet(at_equal))
	break;
      vals.push_back(vars[s]._SYMBptr->feuille[1]);
    }
#endif
    for (int i=0;i<vars.size();++i){
      if (vars[i].is_symb_of_sommet(at_equal))
	vars[i]=vars[i]._SYMBptr->feuille[0];
    }
    if (vals.size()!=vars.size()){
#ifdef DEBUG_SUPPORT
      setsizeerr(gen(vals).print(contextptr)+ " size() != " + gen(vars).print(contextptr));
#endif
      return -RAND_MAX;
    }
    if (debug_ptr(contextptr)->debug_localvars)
      *debug_ptr(contextptr)->debug_localvars=vars;
    const_iterateur it=vals.begin(),itend=vals.end();
    const_iterateur jt=vars.begin();
    gen tmp;
    if (contextptr){
      context * newcontextptr = new context(* contextptr);
      newcontextptr->tabptr = new sym_tab;
      if (contextptr->globalcontextptr)
	newcontextptr->globalcontextptr = contextptr->globalcontextptr;
      else 
	newcontextptr->globalcontextptr = contextptr;
      newcontextptr->previous=contextptr;
      contextptr=newcontextptr;
      if (debug_ptr(contextptr))
	debug_ptr(contextptr)->debug_contextptr=contextptr;
    }
    for (;it!=itend;++it,++jt){
      if (jt->type==_SYMB){
	if (jt->_SYMBptr->sommet==at_check_type){
	  tmp=jt->_SYMBptr->feuille._VECTptr->back();
	  if (is_undef(_check_type(makevecteur(jt->_SYMBptr->feuille._VECTptr->front(),*it),contextptr)))
	    return -RAND_MAX;
	}
	else {
	  if (jt->_SYMBptr->sommet==at_double_deux_points ){
	    tmp=jt->_SYMBptr->feuille._VECTptr->front();
	    if (is_undef(_check_type(makevecteur(jt->_SYMBptr->feuille._VECTptr->back(),*it),contextptr)))
	      return -RAND_MAX;
	  }
	  else { 
	    if (jt->_SYMBptr->sommet==at_of){
	      tmp=jt->_SYMBptr->feuille._VECTptr->front();
	      *logptr(contextptr) << gettext("Invalid variable ")+jt->print(contextptr)+gettext(" using ")+tmp.print(contextptr)+gettext(" instead.");
	    }
	    else 
	      tmp=*jt;
	  }
	}
      }
      else
	tmp=*jt;
      if (tmp.type==_IDNT){
	const char * name=tmp._IDNTptr->id_name;
	int bl=strlen(name);
	gen a=*it;
	if (bl>=2 && name[bl-2]=='_' && (a.type!=_STRNG || a.subtype!=-1)){
	  switch (name[bl-1]){
	  case 'd':
	    if (a.type!=_INT_ && a.type!=_DOUBLE_ && a.type!=_FRAC){
	      *logptr(contextptr) << gettext("Unable to convert to float ")+a.print(contextptr) << endl;
	      return -RAND_MAX;
	    }
	    break;
	  case 'f':
	    if (a.type==_FRAC)
	      break;
	  case 'i': case 'l':
	    if (a.type==_DOUBLE_ && a._DOUBLE_val<=RAND_MAX && a._DOUBLE_val>=-RAND_MAX){
	      int i=int(a._DOUBLE_val);
	      if (i!=a._DOUBLE_val)
		*logptr(contextptr) << gettext("Converting ") << a._DOUBLE_val << gettext(" to integer ") << i << endl;
	      a=i;
	    }
	    else{
	      if (a.type!=_INT_){
		if (a.type!=_ZINT || mpz_sizeinbase(*a._ZINTptr,2)>62){
		  *logptr(contextptr) << gettext("Unable to convert to integer ")+a.print(contextptr) << endl;
		  return -RAND_MAX;
		}
	      }
	    }
	    break;
	  case 'v':
	    if (a.type!=_VECT){
	      *logptr(contextptr) << gettext("Unable to convert to vector ")+a.print(contextptr) << endl;
	      return -RAND_MAX;
	    }
	    break;
	  case 's':
	    if (a.type!=_STRNG)
	      a=string2gen(a.print(contextptr),false);
	  }
	}
	if (contextptr)
	  (*contextptr->tabptr)[tmp._IDNTptr->id_name]=globalize(a);
	else
	  tmp._IDNTptr->push(protection_level,globalize(a));
      }
      else {
	if (tmp.type==_FUNC){
#ifndef NO_STDEXCEPT
	  setsizeerr(gettext("Reserved word:")+tmp.print(contextptr));
#else
	  *logptr(contextptr) << gettext("Reserved word:")+tmp.print(contextptr) << endl;
#endif
	  return -RAND_MAX;
	}
	else {
#ifndef NO_STDEXCEPT
	  setsizeerr(gettext("Not bindable")+tmp.print(contextptr));
#else
	  *logptr(contextptr) << gettext("Not bindable")+tmp.print(contextptr) << endl;
#endif
	  return -RAND_MAX;
	}
      }
    }
    if (!contextptr)
      ++protection_level;
    return protection_level-1;
  }

  bool leave(int protect,vecteur & vars,context * & contextptr){
    iterateur it=vars.begin(),itend=vars.end(),jt,jtend;
    gen tmp;
    if (contextptr){
      if (contextptr->previous){
	context * tmpptr=contextptr;
	contextptr=contextptr->previous;
	if (debug_ptr(contextptr))
	  debug_ptr(contextptr)->debug_contextptr=contextptr;
	if (tmpptr->tabptr){
	  delete tmpptr->tabptr;
	  delete tmpptr;
	  return true;
	}
      }
      return false;
    }
    for (;it!=itend;++it){
      if (it->type==_SYMB && it->_SYMBptr->sommet==at_check_type)
	tmp=it->_SYMBptr->feuille._VECTptr->back();
      else {
	if (it->type==_SYMB && it->_SYMBptr->sommet==at_double_deux_points)
	  tmp=it->_SYMBptr->feuille._VECTptr->front();
	else
	  tmp=*it;
      }
#ifdef DEBUG_SUPPORT
      if (tmp.type!=_IDNT) setsizeerr(gettext("prog.cc/leave"));
#endif    
      if (tmp._IDNTptr->localvalue){
	jt=tmp._IDNTptr->localvalue->begin(),jtend=tmp._IDNTptr->localvalue->end();
	for (;;){
	  if (jt==jtend)
	    break;
	  --jtend;
	  --jtend;
	  if (protect>jtend->val){
	    ++jtend;
	    ++jtend;
	    break;
	  }
	}
	tmp._IDNTptr->localvalue->erase(jtend,tmp._IDNTptr->localvalue->end());
      }
    }
    protection_level=protect;
    return true;
  }
  
  static string printaslocal(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if ( (feuille.type!=_VECT) || (feuille._VECTptr->size()!=2) )
      return string(sommetstr)+('('+feuille.print(contextptr)+')');
    const_iterateur it=feuille._VECTptr->begin(),itend=feuille._VECTptr->end();
    string res;
    gen local_global=*it,locals=gen2vecteur(*it),globals=vecteur(0);
    if (local_global.type==_VECT && local_global._VECTptr->size()==2){ 
      gen f=local_global._VECTptr->front(),b=local_global._VECTptr->back();
      if (f.type!=_IDNT){
	locals=gen2vecteur(f);
	globals=gen2vecteur(b);
      }
    }
    bool python=python_compat(contextptr) && !debug_ptr(contextptr)->debug_mode;
    if (python){
      int & ind=debug_ptr(contextptr)->indent_spaces;
      if (!locals._VECTptr->empty())
	res += string(ind,' ')+"# local "+printaslocalvars(locals,contextptr)+"\n";
      if (!globals._VECTptr->empty())
	res += string(ind,' ')+"global "+printaslocalvars(globals,contextptr)+"\n";
      ++it;
      if (it->is_symb_of_sommet(at_bloc)){
	if (res.size() && res[res.size()-1]=='\n') res=res.substr(0,res.size()-1);
	res += it->print(contextptr)+'\n';
      }
      else {
	if (it->type==_VECT){
	  const_iterateur jt=it->_VECTptr->begin(),jtend=it->_VECTptr->end();
	  for (;jt!=jtend;++jt)
	    res += string(ind,' ')+jt->print(contextptr)+'\n';
	}
	else
	  res += string(ind,' ')+it->print(contextptr)+'\n';
      }
      return res;
    }
    if (!locals._VECTptr->empty()){
      res += indent(contextptr);
      res += "{ local ";
      res += printaslocalvars(locals,contextptr);
      res +=';';
    }
    if (!globals._VECTptr->empty()){
      res += indent(contextptr);
      res += " global ";
      res += printaslocalvars(globals,contextptr);
      res +=';';      
    }
    debug_ptr(contextptr)->indent_spaces +=2;
    ++it;
    for ( ;;){
      gen tmp=*it;
      if (tmp.is_symb_of_sommet(at_bloc))
	tmp=tmp._SYMBptr->feuille;
      if (tmp.type!=_VECT)
	res += indent(contextptr)+tmp.print(contextptr);
      else {
	const_iterateur jt=tmp._VECTptr->begin(),jtend=tmp._VECTptr->end();
	for (;jt!=jtend;++jt){
	  res += indent(contextptr)+jt->print(contextptr);
	  res += "; " ;
	}
      }
      ++it;
      if (it==itend){
	debug_ptr(contextptr)->indent_spaces -= 2;
	if (!locals._VECTptr->empty()) res += indent(contextptr)+"}";
	return res;
      }
      else
	res +="; ";
    }
  }
  gen symb_local(const gen & a,const gen & b,GIAC_CONTEXT){
    gen newa,newb;
    replace_keywords(a,b,newa,newb,contextptr);
    return symbolic(at_local,makesequence(newa,newb));
  }
  gen symb_local(const gen & args,GIAC_CONTEXT){
    if (args.type==_VECT && args._VECTptr->size()==2)
      return symb_local(args._VECTptr->front(),args._VECTptr->back(),contextptr);
    return symbolic(at_local,args);
  }

  gen _local(const gen & args,const context * contextptr) {
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symb_local(args,contextptr);
    int s=int(args._VECTptr->size());
    if (s!=2)
      return gensizeerr(gettext("Local must have 2 args"));
    // Initialization
    gen vars=args._VECTptr->front();
    if (vars.type==_VECT && vars._VECTptr->size()==2 && vars._VECTptr->front().type!=_IDNT)
      vars = vars._VECTptr->front();
    if (vars.type!=_VECT)
      vars=makevecteur(vars);
    vecteur names,values;
    iterateur it=vars._VECTptr->begin(),itend=vars._VECTptr->end();
    names.reserve(itend-it);
    values.reserve(itend-it);
    for (;it!=itend;++it){
      if (it->type==_IDNT){
	names.push_back(*it);
#if 1
	gen err=string2gen(gettext("Unitialized local variable ")+it->print(contextptr),false);
	err.subtype=-1;
	values.push_back(err);
#else
	values.push_back(0);
#endif
	continue;
      }
      if ( (it->type!=_SYMB) || (it->_SYMBptr->sommet!=at_sto && it->_SYMBptr->sommet!=at_equal))
	return gentypeerr(contextptr);
      gen nom=it->_SYMBptr->feuille._VECTptr->back();
      gen val=it->_SYMBptr->feuille._VECTptr->front();
      if (it->_SYMBptr->sommet==at_equal)
	swapgen(nom,val);
      val=val.eval(eval_level(contextptr),contextptr);
      if (nom.type!=_IDNT)
	return gentypeerr(contextptr);
      names.push_back(nom);
      values.push_back(val);
    }
    context * newcontextptr = (context *) contextptr;
    int protect=giac_bind(values,names,newcontextptr);
    gen prog=args._VECTptr->back(),res,newres;
    if (protect!=-RAND_MAX){
      if (prog.type!=_VECT){
	prog=equaltosto(prog,contextptr);
	++debug_ptr(newcontextptr)->current_instruction;
	if (debug_ptr(newcontextptr)->debug_mode){
	  debug_loop(res,newcontextptr);
	  if (!is_undef(res)){
	    if (!prog.in_eval(eval_level(newcontextptr),res,newcontextptr))
	      res=prog;
	  }
	}
	else {
	  if (!prog.in_eval(eval_level(newcontextptr),res,newcontextptr))
	    res=prog;
	}
      }
      else {
	it=prog._VECTptr->begin(),itend=prog._VECTptr->end();
	bool findlabel=false;
	gen label;
	for (;!ctrl_c && !interrupted && it!=itend;++it){
#ifdef TIMEOUT
	  control_c();
#endif
	  ++debug_ptr(newcontextptr)->current_instruction;
	  // cout << *it << endl;
	  if (debug_ptr(newcontextptr)->debug_mode){
	    debug_loop(res,newcontextptr);
	    if (!is_undef(res)){
	      if (!findlabel){
		if (!equaltosto(*it,contextptr).in_eval(eval_level(newcontextptr),res,newcontextptr))
		  res=*it;
	      }
	      else
		res=*it;
	    }
	  }
	  else {
	    if (!findlabel){
	      if (!equaltosto(*it,contextptr).in_eval(eval_level(newcontextptr),res,newcontextptr))
		res=*it;
	    }
	    else
	      res=*it;
	  }
	  if (res.type==_SYMB){
	    unary_function_ptr & u=res._SYMBptr->sommet;
	    if (findlabel && u==at_label && label==res._SYMBptr->feuille)
	      findlabel=false;
	    if (!findlabel && u==at_goto){
	      findlabel=true;
	      label=res._SYMBptr->feuille;
	    }
	  }
	  if (findlabel && it+1==itend)
	    it=prog._VECTptr->begin()-1;
	  if (!findlabel && is_return(res,newres) ){
	    // res=newres;
	    break;
	  }
	}
      }
      leave(protect,names,newcontextptr);
    }
    else
      return gensizeerr(contextptr);
    return res;
  }

  static const char _local_s []="local";
  static define_unary_function_eval2_index (85,__local,&_local,_local_s,&printaslocal);
  define_unary_function_ptr5( at_local ,alias_at_local,&__local,_QUOTE_ARGUMENTS,0);

  static string printasreturn(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    return sommetstr+("("+feuille.print(contextptr)+")");
  }
  static const char _return_s []="return";
  static define_unary_function_eval2_index (86,__return,&symb_return,_return_s,&printasreturn);
  define_unary_function_ptr( at_return ,alias_at_return ,&__return);

  static string printastry_catch(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if ( (feuille.type!=_VECT) || (feuille._VECTptr->size()<3) )
      return string(sommetstr)+('('+feuille.print(contextptr)+')');
    const_iterateur it=feuille._VECTptr->begin();//,itend=feuille._VECTptr->end();
    string res;
    if (feuille._VECTptr->size()==4){
      res = "IFERR ";
      res += printasinnerbloc(*it,contextptr);
      ++it;
      ++it;
      res += " THEN ";
      res += printasinnerbloc(*it,contextptr);
      ++it;
      res += " ELSE ";
      res += printasinnerbloc(*it,contextptr);
      res += " END";
      return res;
    }
    res += "try ";
    res += it->print(contextptr);
    ++it;
    if (res[res.size()-1]!='}')
      res += "; ";
    res += "catch(" + it->print(contextptr) + ")";
    ++it;
    res += it->print(contextptr);
    if (res[res.size()-1]!='}')
      res += "; ";
    return res;
  }
  
  gen symb_try_catch(const gen & args){
    return symbolic(at_try_catch,args);
  }
  gen _try_catch(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symb_try_catch(args);
    int args_size=int(args._VECTptr->size());
    if (args_size!=3 && args_size!=4)
      return gensizeerr(gettext("Try_catch must have 3 or 4 args"));
    gen res;
    int saveprotect=protection_level;
    vector< vector<int> > save_sst_at_stack(debug_ptr(contextptr)->sst_at_stack);
    vecteur save_args_stack(debug_ptr(contextptr)->args_stack);
    vector<int> save_current_instruction_stack=debug_ptr(contextptr)->current_instruction_stack;
    int save_current_instruction=debug_ptr(contextptr)->current_instruction;
    bool do_else_try=args_size==4;
#ifndef NO_STDEXCEPT
    try {
      ++debug_ptr(contextptr)->current_instruction;
      if (debug_ptr(contextptr)->debug_mode)
	debug_loop(res,contextptr);
      res=args._VECTptr->front().eval(eval_level(contextptr),contextptr);
    }
    catch (std::runtime_error & error ){
      last_evaled_argptr(contextptr)=NULL;
      ++debug_ptr(contextptr)->current_instruction;
      if (debug_ptr(contextptr)->debug_mode)
	debug_loop(res,contextptr);
      // ??? res=args._VECTptr->front().eval(eval_level(contextptr),contextptr);
      do_else_try=false;
      if (!contextptr)
	protection_level=saveprotect;
      debug_ptr(contextptr)->sst_at_stack=save_sst_at_stack;
      debug_ptr(contextptr)->args_stack=save_args_stack;
      debug_ptr(contextptr)->current_instruction_stack=save_current_instruction_stack;
      gen id=(*(args._VECTptr))[1];
      string er(error.what());
      er = '"'+er+'"'; // FIXME? string2gen(er,false) instead?
      gen tmpsto;
      if (id.type==_IDNT)
	tmpsto=sto(gen(er,contextptr),id,contextptr);
      if (is_undef(tmpsto)) return tmpsto;
      debug_ptr(contextptr)->current_instruction=save_current_instruction;
      increment_instruction(args._VECTptr->front(),contextptr);
      ++debug_ptr(contextptr)->current_instruction;
      if (debug_ptr(contextptr)->debug_mode)
	debug_loop(res,contextptr);
      res=(*args._VECTptr)[2].eval(eval_level(contextptr),contextptr);
    }
#else
    ++debug_ptr(contextptr)->current_instruction;
    if (debug_ptr(contextptr)->debug_mode)
      debug_loop(res,contextptr);
    if (is_undef(res)) return res;
    res=args._VECTptr->front().eval(eval_level(contextptr),contextptr);
    if (is_undef(res)){
      do_else_try=false;
      if (!contextptr)
	protection_level=saveprotect;
      debug_ptr(contextptr)->sst_at_stack=save_sst_at_stack;
      debug_ptr(contextptr)->args_stack=save_args_stack;
      debug_ptr(contextptr)->current_instruction_stack=save_current_instruction_stack;
      gen id=(*(args._VECTptr))[1];
      string er(gen2string(res));
      er = '"'+er+'"';
      gen tmpsto;
      if (id.type==_IDNT)
	tmpsto=sto(gen(er,contextptr),id,contextptr);
      if (is_undef(tmpsto)) return tmpsto;
      debug_ptr(contextptr)->current_instruction=save_current_instruction;
      increment_instruction(args._VECTptr->front(),contextptr);
      ++debug_ptr(contextptr)->current_instruction;
      if (debug_ptr(contextptr)->debug_mode){
	debug_loop(res,contextptr);
	if (is_undef(res)) return res;
      }
      res=(*args._VECTptr)[2].eval(eval_level(contextptr),contextptr);
    }
#endif
    if (do_else_try){
      if ((res.type==_SYMB && res._SYMBptr->sommet==at_return) || (res.type==_FUNC && (res==at_return || res==at_break || res==at_continue)))
	;
      else
	res=args._VECTptr->back().eval(eval_level(contextptr),contextptr);
    }
    debug_ptr(contextptr)->current_instruction=save_current_instruction;
    increment_instruction(args._VECTptr->front(),contextptr);
    increment_instruction(args._VECTptr->back(),contextptr);
    return res;
  }
  static const char _try_catch_s []="try_catch";
  static define_unary_function_eval2_index (177,__try_catch,&_try_catch,_try_catch_s,&printastry_catch);
  define_unary_function_ptr5( at_try_catch ,alias_at_try_catch,&__try_catch,_QUOTE_ARGUMENTS,0);

  static gen feuille_(const gen & g,const gen & interval,GIAC_CONTEXT){
    vecteur v;
    if (g.type==_SYMB){
      gen & f=g._SYMBptr->feuille;
      if (f.type==_VECT)
	v=*f._VECTptr;
      else
	v=vecteur(1,f);
    }
    else {
      if (g.type==_VECT)
	v=*g._VECTptr;
      else
	v=vecteur(1,g);
    }
    int s=int(v.size());
    if (interval.type==_INT_){
      int i=interval.val-array_start(contextptr); //-(xcas_mode(contextptr)!=0);
      if (i==-1 && g.type==_SYMB)
	return g._SYMBptr->sommet;
      if (i<0 || i>=s)
	return gendimerr(contextptr);
      return v[i];
    }
    if (interval.is_symb_of_sommet(at_interval)&& interval._SYMBptr->feuille.type==_VECT){
      vecteur & w=*interval._SYMBptr->feuille._VECTptr;
      if (w.size()!=2 || w.front().type!=_INT_ || w.back().type!=_INT_)
	return gentypeerr(contextptr);
      int i=w.front().val,j=w.back().val;
      if (i>j)
	return gen(vecteur(0),_SEQ__VECT);
      if (array_start(contextptr)){
	--i;
	--j;
      }
      if (i<0 || i>=s || j<0 || j>=s)
	return gendimerr(contextptr);
      return gen(vecteur(v.begin()+i,v.begin()+j+1),_SEQ__VECT);
    }
    return gensizeerr(contextptr);
  }
  gen _feuille(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_VECT){
      if (args.subtype==_SEQ__VECT && args._VECTptr->size()==2)
	return feuille_(args._VECTptr->front(),args._VECTptr->back(),contextptr);
      return gen(*args._VECTptr,_SEQ__VECT);
    }
    if (args.type!=_SYMB)
      return args;
    gen tmp=args._SYMBptr->feuille;
    if (tmp.type==_VECT)
      tmp.subtype=_SEQ__VECT;
    return tmp;
  }
  static const char _feuille_s []="op";
  static define_unary_function_eval2 (__feuille,&_feuille,_feuille_s,&printassubs);
  define_unary_function_ptr5( at_feuille ,alias_at_feuille,&__feuille,0,true);
  
  gen _sommet(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_SYMB)
      return at_id;
    int nargs;
    if (args._SYMBptr->feuille.type==_VECT)
        nargs=int(args._SYMBptr->feuille._VECTptr->size());
    else
        nargs=1;
    return gen(args._SYMBptr->sommet,nargs);
  }
  static const char _sommet_s []="sommet";
  static define_unary_function_eval (__sommet,&_sommet,_sommet_s);
  define_unary_function_ptr5( at_sommet ,alias_at_sommet,&__sommet,0,true);

  
  // static gen symb_append(const gen & args){  return symbolic(at_append,args);  }
  gen _append(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if ( args.type!=_VECT || !args._VECTptr->size() )
      return gensizeerr(contextptr);
    const_iterateur it=args._VECTptr->begin(),itend=args._VECTptr->end();
    if (itend-it==2 && it->type==_STRNG && (it+1)->type==_STRNG)
      return string2gen(*it->_STRNGptr+*(it+1)->_STRNGptr,false);
    if (it->type!=_VECT)
      return gensizeerr(contextptr);
    vecteur v(*it->_VECTptr);
    int subtype=it->subtype;
    ++it;
    if (v.size()+(itend-it)>LIST_SIZE_LIMIT)
      return gendimerr(contextptr);
    for (;it!=itend;++it)
      v.push_back(*it);
    return gen(v,subtype);
  }
  static const char _append_s []="append";
  static define_unary_function_eval (__append,&_append,_append_s);
  define_unary_function_ptr5( at_append ,alias_at_append,&__append,0,true);

  // static gen symb_prepend(const gen & args){  return symbolic(at_prepend,args); }
  gen _prepend(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_VECT && args._VECTptr->size()==2 && args._VECTptr->front().type==_STRNG && args._VECTptr->back().type==_STRNG)
      return args._VECTptr->back()+args._VECTptr->front();
    if ( (args.type!=_VECT) || (!args._VECTptr->size()) || (args._VECTptr->front().type!=_VECT) )
      return gensizeerr(contextptr);
    gen debut=args._VECTptr->front();
    return gen(mergevecteur(cdr_VECT(*args._VECTptr),*debut._VECTptr),debut.subtype);
  }
  static const char _prepend_s []="prepend";
  static define_unary_function_eval (__prepend,&_prepend,_prepend_s);
  define_unary_function_ptr5( at_prepend ,alias_at_prepend,&__prepend,0,true);

  gen _contains(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if ( (args.type!=_VECT) || (args._VECTptr->size()!=2))
      return gensizeerr(contextptr);
    gen a=args._VECTptr->front(),b=args._VECTptr->back();
    if (a.type!=_VECT){
      // if (a.type==_REAL) return contains(a,b);
      if (b==cst_i)
	return has_i(a);
      return gensizeerr(contextptr);
    }
    return equalposcomp(*a._VECTptr,b);
  }
  static const char _contains_s []="contains";
  static define_unary_function_eval (__contains,&_contains,_contains_s);
  define_unary_function_ptr5( at_contains ,alias_at_contains,&__contains,0,true);

  // check if a set A is included in a set B
  gen _is_included(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if ( (args.type!=_VECT) || (args._VECTptr->size()!=2) || (args._VECTptr->front().type!=_VECT) || (args._VECTptr->back().type!=_VECT) )
      return gensizeerr(contextptr);
    vecteur a(*args._VECTptr->front()._VECTptr);
    vecteur b(*args._VECTptr->back()._VECTptr);
    islesscomplexthanf_sort(b.begin(),b.end());
    for (unsigned i=0;i<a.size();++i){
      if (!binary_search(b.begin(),b.end(),a[i],islesscomplexthanf))
	return 0;
    }
    return 1;
  }
  static const char _is_included_s []="is_included";
  static define_unary_function_eval (__is_included,&_is_included,_is_included_s);
  define_unary_function_ptr5( at_is_included ,alias_at_is_included,&__is_included,0,true);

  static gen symb_select(const gen & args){
    return symbolic(at_select,args);
  }
  static gen symb_remove(const gen & args){
    return symbolic(at_remove,args);
  }
  static gen select_remove(const gen & args,bool selecting,const context * contextptr){
    if ( (args.type!=_VECT) || (args._VECTptr->size()<2)){
      if (selecting)
	return symb_select(args);
      else
	return symb_remove(args);
    }
    gen v((*(args._VECTptr))[1]);
    int subtype;
    gen fn=0;
    if (v.type==_SYMB){
      if (v._SYMBptr->feuille.type==_VECT)
	v=v._SYMBptr->feuille;
      else
	v=makevecteur(v._SYMBptr->feuille);
      subtype=-1;
      fn=v;
    }
    else
      subtype=v.subtype;
    gen f(args._VECTptr->front());
    if ( (v.type!=_VECT) && (v.type!=_SYMB)){
      if (selecting)
	return symb_select(args);
      else {
	if (f.type==_VECT){ // remove 1st occurence of v
	  vecteur w=*f._VECTptr;
	  for (unsigned i=0;i<w.size();++i){
	    if (w[i]==v){
	      w.erase(w.begin()+i);
	      return gen(w,f.subtype);
	    }
	  }
	  return gensizeerr(contextptr);
	}
	return symb_remove(args);
      }
    }
    bool prog=f.is_symb_of_sommet(at_program);
    vecteur otherargs(args._VECTptr->begin()+1,args._VECTptr->end());
    const_iterateur it=v._VECTptr->begin(),itend=v._VECTptr->end();
    vecteur res;
    res.reserve(itend-it);
    if (otherargs.size()==1){
      for (;it!=itend;++it){
	if (prog){
	  if (is_zero(f(*it,contextptr))!=selecting)
	    res.push_back(*it);
	}
	else {
	  if ((*it==f)==selecting )
	    res.push_back(*it);
	}
      }
    }
    else {
      for (;it!=itend;++it){
	otherargs.front()=*it;
	if (is_zero(f(otherargs,contextptr))!=selecting)
	  res.push_back(*it);
      }
    }
    if (subtype<0 && fn.type==_SYMB)
      return symbolic(fn._SYMBptr->sommet,res);
    else
      return gen(res,subtype);
  }
  gen _select(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return select_remove(args,1,contextptr);
  }
  static const char _select_s []="select";
  static define_unary_function_eval (__select,&_select,_select_s);
  define_unary_function_ptr5( at_select ,alias_at_select,&__select,0,true);

  static const char _filter_s []="filter";
  static define_unary_function_eval (__filter,&_select,_filter_s);
  define_unary_function_ptr5( at_filter ,alias_at_filter,&__filter,0,true);

  gen _remove(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return select_remove(args,0,contextptr);
  }
  static const char _remove_s []="remove";
  static define_unary_function_eval (__remove,&_remove,_remove_s);
  define_unary_function_ptr5( at_remove ,alias_at_remove,&__remove,0,true);

  static string printnostring(const gen & g,GIAC_CONTEXT){
    if (g.type==_STRNG)
      return *g._STRNGptr;
    else
      return g.print(contextptr);
  }
  static gen symb_concat(const gen & args){
    return symbolic(at_concat,args);
  }
  gen concat(const gen & g,bool glue_lines,GIAC_CONTEXT){
    if (g.type!=_VECT)
      return symb_concat(g);
    vecteur & v=*g._VECTptr;
    if (v.size()>2){
      gen h=concat(makesequence(v[0],v[1]),glue_lines,contextptr);
      for (unsigned i=2;i<v.size();++i){
	h=concat(makesequence(h,v[i]),glue_lines,contextptr);
      }
      return h;
    }
    if (v.size()!=2){
      if (g.subtype==_SEQ__VECT)
	return g;
      return symb_concat(g);
    }
    gen v0=v[0],v1=v[1];
    if (v0.type==_VECT && v1.type==_VECT){
      if (!glue_lines && v1.subtype!=_SEQ__VECT && ckmatrix(v0) && ckmatrix(v1) && v0._VECTptr->size()==v1._VECTptr->size() )
	return gen(mtran(mergevecteur(mtran(*v0._VECTptr),mtran(*v1._VECTptr))));
      else
	return gen(mergevecteur(*v0._VECTptr,*v1._VECTptr),v0.subtype);
    }
    if (v0.type==_VECT)
      return gen(mergevecteur(*v0._VECTptr,vecteur(1,v1)),v0.subtype);
    if (v1.type==_VECT)
      return gen(mergevecteur(vecteur(1,v0),*v1._VECTptr),v1.subtype);
    if ( (v0.type==_STRNG) || (v1.type==_STRNG) )
      return string2gen(printnostring(v0,contextptr) + printnostring(v1,contextptr),false);
    return 0;
  }
  gen _concat(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return concat(args,false,contextptr);
  }
  static const char _concat_s []="concat";
  static define_unary_function_eval (__concat,&_concat,_concat_s);
  define_unary_function_ptr5( at_concat ,alias_at_concat,&__concat,0,true);

  static const char _extend_s []="extend";
  static define_unary_function_eval (__extend,&_concat,_extend_s);
  define_unary_function_ptr5( at_extend ,alias_at_extend,&__extend,0,true);

  static gen symb_option(const gen & args){
    return symbolic(at_option,args);
  }
  gen _option(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return symb_option(args);
  }
  static const char _option_s []="option";
  static define_unary_function_eval (__option,&_option,_option_s);
  define_unary_function_ptr( at_option ,alias_at_option ,&__option);

  static string printascase(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if ( (feuille.type!=_VECT) || (feuille._VECTptr->size()!=2) || (feuille._VECTptr->back().type!=_VECT))
      return string(sommetstr)+('('+feuille.print(contextptr)+')');
    string res("switch (");
    res += feuille._VECTptr->front().print(contextptr);
    res += "){";
    debug_ptr(contextptr)->indent_spaces +=2;
    const_iterateur it=feuille._VECTptr->back()._VECTptr->begin(),itend=feuille._VECTptr->back()._VECTptr->end();
    for (;it!=itend;++it){
      ++it;
      if (it==itend){
	res += indent(contextptr)+"default:";
	--it;
	debug_ptr(contextptr)->indent_spaces += 2;
	res += indent(contextptr)+it->print(contextptr);
	debug_ptr(contextptr)->indent_spaces -= 2;
	break;
      }
      res += indent(contextptr)+"case "+(it-1)->print(contextptr)+":";
      debug_ptr(contextptr)->indent_spaces += 2;
      res += indent(contextptr)+it->print(contextptr);
      debug_ptr(contextptr)->indent_spaces -=2;
    }
    debug_ptr(contextptr)->indent_spaces -=2;
    res+=indent(contextptr)+"}";
    return res;
  }
  gen symb_case(const gen & args){
    return symbolic(at_case,args);
  }
  gen symb_case(const gen & a,const gen & b){
    return symbolic(at_case,makesequence(a,b));
  }
  gen _case(const gen & args,GIAC_CONTEXT){ // FIXME DEBUGGER
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if ( (args.type!=_VECT) || (args._VECTptr->size()!=2) || (args._VECTptr->back().type!=_VECT) )
      return symb_case(args);
    gen expr=args._VECTptr->front().eval(eval_level(contextptr),contextptr),res=undef,oldres,newres;
    const_iterateur it=args._VECTptr->back()._VECTptr->begin(),itend=args._VECTptr->back()._VECTptr->end();
    for (;it!=itend;){
      if (it+1==itend){
	res=it->eval(eval_level(contextptr),contextptr);
	break;
      }
      if (expr==it->eval(eval_level(contextptr),contextptr)){
	++it;
	oldres=res;
	res=it->eval(eval_level(contextptr),contextptr);
	if (res==symbolic(at_break,zero)){
	  res=oldres;
	  break;
	}
	if (res.is_symb_of_sommet(at_return))
	  break;
      }
      else
	++it;
      if (it!=itend)
	++it;
    }
    return res;
  }
  static const char _case_s []="case";
  static define_unary_function_eval2_index (123,__case,&_case,_case_s,&printascase);
  define_unary_function_ptr5( at_case ,alias_at_case,&__case,_QUOTE_ARGUMENTS,0);

  static gen symb_rand(const gen & args){
    return symbolic(at_rand,args);
  }
  static gen rand_integer_interval(const gen & x1,const gen & x2,GIAC_CONTEXT){
    gen x2x1=x2-x1;
    if (!is_positive(x2x1,contextptr))
      return rand_integer_interval(x2,x1,contextptr);
    int n=x2x1.bindigits()/gen(rand_max2).bindigits()+1;
    gen res=zero;
#ifndef USE_GMP_REPLACEMENTS
    if (unsigned(rand_max2)==(1u<<31)-1){
      mpz_t tmp;
      mpz_init(tmp);
      for (int i=0;i<n;++i){
	mpz_mul_2exp(tmp,tmp,31);
	mpz_add_ui(tmp,tmp,giac_rand(contextptr));
      }
      if (x2x1.type==_INT_)
	mpz_mul_si(tmp,tmp,x2x1.val);
      else
	mpz_mul(tmp,tmp,*x2x1._ZINTptr);
      mpz_tdiv_q_2exp(tmp,tmp,31*n);
      if (x1.type==_INT_){
	if (x1.val>0) mpz_add_ui(tmp,tmp,x1.val); else mpz_sub_ui(tmp,tmp,-x1.val);
      }
      else
	mpz_add(tmp,tmp,*x1._ZINTptr);
      res=tmp;
      mpz_clear(tmp);
      return res;
    }
#endif
#ifdef FXCG
    gen rand_max_plus_one=gen(rand_max2)+1;
#else
    static gen rand_max_plus_one=gen(rand_max2)+1;
#endif
    // Make n random numbers
    for (int i=0;i<n;++i)
      res=rand_max_plus_one*res+giac_rand(contextptr);
    // Now res is in [0,(RAND_MAX+1)^n-1]
    // Rescale in x1..x2
    return x1+_iquo(makevecteur(res*x2x1,pow(rand_max_plus_one,n)),contextptr);
  }
  gen rand_interval(const vecteur & v,bool entier,GIAC_CONTEXT){
    gen x1=v.front(),x2=v.back();
    if (x1==x2)
      return x1;
    if ((entier || xcas_mode(contextptr)==1) && is_integer(x1) && is_integer(x2) )
      return rand_integer_interval(x1,x2,contextptr);
#ifdef FXCG
    gen rand_max_plus_one=gen(rand_max2)+1;
#else
    static gen rand_max_plus_one=gen(rand_max2)+1;
#endif
#ifdef HAVE_LIBMPFR
    if (x1.type==_REAL && x2.type==_REAL){
      int n=mpfr_get_prec(x1._REALptr->inf);
      int nr=int(n*std::log(2.0)/std::log(rand_max2+1.0));
      gen xr=0;
      for (int i=0;i<=nr;++i){
	xr=xr*rand_max_plus_one+giac_rand(contextptr);
      }
      return x1+((x2-x1)*xr)/pow(rand_max_plus_one,nr+1);
    }
#endif
    gen x=evalf_double(x1,1,contextptr),y=evalf_double(x2,1,contextptr);
    if ( (x.type==_DOUBLE_) && (y.type==_DOUBLE_) ){
      double xd=x._DOUBLE_val,yd=y._DOUBLE_val;
      double xr= (giac_rand(contextptr)/evalf_double(rand_max_plus_one,1,contextptr)._DOUBLE_val)*(yd-xd)+xd;
      return xr;
    }
    return symb_rand(gen(v,_SEQ__VECT));
  }

  void shuffle(vector<int> & temp,GIAC_CONTEXT){
    int n=int(temp.size());
    // source wikipedia Fisher-Yates shuffle article
    for (int i=0;i<n-1;++i){
      // j ← random integer such that i ≤ j < n
      // exchange a[i] and a[j]
      int j=int(i+(giac_rand(contextptr)/double(rand_max2))*(n-i));
      std::swap(temp[i],temp[j]);
    }
  }

  vector<int> rand_k_n(int k,int n,bool sorted,GIAC_CONTEXT){
    if (k<=0 || n<=0 || k>n)
      return vector<int>(0);
    if (//n>=65536 && 
	k*double(k)<=n/4){
      vector<int> t(k),ts(k); 
      for (int essai=20;essai>=0;--essai){
	int i;
	for (i=0;i<k;++i)
	  ts[i]=t[i]=int(giac_rand(contextptr)/double(rand_max2)*n);
	sort(ts.begin(),ts.end());
	for (i=1;i<k;++i){
	  if (ts[i]==ts[i-1])
	    break;
	}
	if (i==k)
	  return sorted?ts:t;
      }
    }
    if (k>=n/3 || (sorted && k*std::log(double(k))>n) ){
      vector<int> t; t.reserve(k);
      // (algorithm suggested by O. Garet)
      while (n>0){
	int r=int(giac_rand(contextptr)/double(rand_max2)*n);
	if (r<n-k) // (n-k)/n=proba that the current n is not in the list
	  --n;
	else {
	  --n;
	  t.push_back(n);
	  --k;
	}
      }
      if (sorted)
	reverse(t.begin(),t.end());
      else
	shuffle(t,contextptr);
      return t;
    }
    vector<bool> tab(n,true);
    vector<int> v(k);
    for (int j=0;j<k;++j){
      int r=-1;
      for (;;){
	r=int(giac_rand(contextptr)/double(rand_max2)*n);
	if (tab[r]){ tab[r]=false; break; }
      }
      v[j]=r;
    }
    if (sorted)
      sort(v.begin(),v.end());
    return v;
  }
  
  vector<int> randperm(const int & n,GIAC_CONTEXT){
    //renvoie une permutation au hasard de long n
    vector<int> temp(n);
    for (int k=0;k<n;k++) 
      temp[k]=k;
    shuffle(temp,contextptr);
    return temp;
  }

  static gen rand_n_in_list(int n,const vecteur & v,GIAC_CONTEXT){
    n=absint(n);
    if (signed(v.size())<n)
      return gendimerr(contextptr);
#if 1
    vector<int> w=rand_k_n(n,int(v.size()),false,contextptr);
    vecteur res(n);
    for (int i=0;i<n;++i)
      res[i]=v[w[i]];
    return res;
#else
    // would be faster with randperm
    vecteur w(v);
    vecteur res;
    for (int i=0;i<n;++i){
      int tmp=int((double(giac_rand(contextptr))*w.size())/rand_max2);
      res.push_back(w[tmp]);
      w.erase(w.begin()+tmp);
    }
    return res;
#endif
  }
  gen _rand(const gen & args,GIAC_CONTEXT){
    int argsval;
    if (args.type==_INT_ && (argsval=args.val)){
#ifndef FXCG
      global * ptr; int c;
      if (argsval>0 && contextptr && (ptr=contextptr->globalptr) && ptr->_xcas_mode_!=3 && (c=ptr->_calc_mode_)!=-38 && c!=38) {
	tinymt32_t * rs=&ptr->_rand_seed;
	unsigned r;
	for (;;){
	  r=tinymt32_generate_uint32(rs) >> 1;
	  if (!(r>>31))
	    break;
	}
	return int(argsval*(r*inv_rand_max2_p1));
      }
#endif
      if (argsval<0)
	return -(xcas_mode(contextptr)==3)+int(argsval*(giac_rand(contextptr)/(rand_max2+1.0)));
      else
	return (xcas_mode(contextptr)==3 || abs_calc_mode(contextptr)==38)+int(argsval*(giac_rand(contextptr)/(rand_max2+1.0)));
    }
    if (args.type==_STRNG &&  args.subtype==-1) return  args;
    if (is_zero(args) && args.type!=_VECT)
      return giac_rand(contextptr);
    if (args.type==_ZINT)
      return rand_integer_interval(zero,args,contextptr);
    if (args.type==_FRAC)
      return _rand(args._FRACptr->num,contextptr)/args._FRACptr->den;
    if (args.type==_USER)
      return args._USERptr->rand(contextptr);
    if (args.is_symb_of_sommet(at_rootof))
      return vranm(1,args,contextptr)[0];
    int nd=is_distribution(args);
    if (nd==1 && args.type==_FUNC)
      return randNorm(contextptr);
    if (args.is_symb_of_sommet(at_exp))
      return _randexp(args._SYMBptr->feuille,contextptr);
    if (nd && args.type==_SYMB){
      vecteur v=gen2vecteur(args._SYMBptr->feuille);
      if (nd==1){
	if (v.size()!=2)
	  return gensizeerr(contextptr);
	return randNorm(contextptr)*v[1]+v[0];
      }
      if (nd==2){
	if (v.size()!=2 || !is_integral(v[0]) || v[0].type!=_INT_ || (v[1]=evalf_double(v[1],1,contextptr)).type!=_DOUBLE_)
	  return gensizeerr(contextptr);
	return randbinomial(v[0].val,v[1]._DOUBLE_val,contextptr);
      }
      double p=giac_rand(contextptr)/(rand_max2+1.0);
      v.push_back(p);
      return icdf(nd)(gen(v,_SEQ__VECT),contextptr);
    }
    if (args.type==_VECT){ 
      if (args._VECTptr->empty()){
	if (xcas_mode(contextptr)==1)
	  return giac_rand(contextptr);
	else
	  return giac_rand(contextptr)/(rand_max2+1.0);
      }
      if (args.subtype==0){
	double d=giac_rand(contextptr)*double(args._VECTptr->size())/(rand_max2+1.0);
	return (*args._VECTptr)[int(d)];
      }
      vecteur & v=*args._VECTptr;
      int s=int(v.size());
      if ((nd=is_distribution(v[0]))){
	if (s==1)
	  return _rand(v[0],contextptr);
	return _rand(v[0] (gen(vecteur(v.begin()+1,v.end()),_SEQ__VECT),contextptr),contextptr);
      }
      if (s==2){ 
	if (v[0]==at_exp)
	  return _randexp(v[1],contextptr);
	if (v.front().type==_INT_ && v.back().type==_VECT){ // rand(n,list) choose n in list
	  return rand_n_in_list(v.front().val,*v.back()._VECTptr,contextptr);
	}
	if ( (v.back().type==_SYMB) && (v.back()._SYMBptr->sommet==at_interval) ){
	  // arg1=loi, arg2=intervalle
	}
	if (v[0].type==_DOUBLE_ && v[1].type==_INT_)
	  return _randvector(makesequence(v[1],symb_interval(0,1)),contextptr);
	return rand_interval(v,args.subtype==0,contextptr);
      }
      if (s==3 && v[0].type==_DOUBLE_ && v[1].type==_INT_ && v[2].type==_INT_)
	return _ranm(makesequence(v[1],v[2],symb_interval(0,1)),contextptr);
      if (s==3 && v[0].type==_INT_ && v[1].type==_INT_ && v[2].type==_INT_){ 
	// 3 integers expected, rand(n,min,max) choose n in min..max
	int n=v[0].val;
	int m=v[1].val;
	int M=v[2].val;
	if (m>M){ int tmp=m; m=M; M=tmp; }
#if 1
	vector<int> v=rand_k_n(n,M-m+1,false,contextptr);
	if (v.empty()) return gendimerr(contextptr);
	for (int i=0;i<n;++i)
	  v[i] += m;
	vecteur res;
	vector_int2vecteur(v,res);
	return res;
#else
	vecteur v;
	for (int i=m;i<=M;++i) v.push_back(i);
	return rand_n_in_list(n,v,contextptr);
#endif
      }
    }
    if ( (args.type==_SYMB) && (args._SYMBptr->sommet==at_interval) ){
      vecteur & v=*args._SYMBptr->feuille._VECTptr;
      return symb_program(vecteur(0),vecteur(0),symb_rand(gen(v,_SEQ__VECT)),contextptr);
      // return rand_interval(v);
    }
    return symb_rand(args);
  }
  static const char _rand_s []="rand";
  static define_unary_function_eval (__rand,&_rand,_rand_s);
  define_unary_function_ptr5( at_rand ,alias_at_rand,&__rand,0,true);

  static const char _random_s []="random";
  static define_unary_function_eval (__random,&_rand,_random_s);
  define_unary_function_ptr5( at_random ,alias_at_random,&__random,0,true);

  gen _randint(const gen & args,GIAC_CONTEXT){
    if (args.type==_INT_)
      return (abs_calc_mode(contextptr)==38?0:1)+_rand(args,contextptr);
    if (args.type!=_VECT || args._VECTptr->size()!=2)
      return gensizeerr(contextptr);
    gen a=args._VECTptr->front(),b=args._VECTptr->back();
    if (!is_integral(a) || !is_integral(b))
      return gentypeerr(contextptr);
    return (abs_calc_mode(contextptr)==38?a-1:a)+_rand(b-a+1,contextptr);
  }
  static const char _randint_s []="randint";
  static define_unary_function_eval (__randint,&_randint,_randint_s);
  define_unary_function_ptr5( at_randint ,alias_at_randint,&__randint,0,true);

  gen _randrange(const gen & args,GIAC_CONTEXT){
    if (args.type==_INT_)
      return (abs_calc_mode(contextptr)==38?-1:0)+_rand(args,contextptr);
    if (args.type!=_VECT || args._VECTptr->size()!=2)
      return gensizeerr(contextptr);
    gen a=args._VECTptr->front(),b=args._VECTptr->back();
    if (!is_integral(a) || !is_integral(b))
      return gentypeerr(contextptr);
    return (abs_calc_mode(contextptr)==38?a-1:a)+_rand(b-a,contextptr);
  }
  static const char _randrange_s []="randrange";
  static define_unary_function_eval (__randrange,&_randrange,_randrange_s);
  define_unary_function_ptr5( at_randrange ,alias_at_randrange,&__randrange,0,true);

  gen _choice(const gen & args,GIAC_CONTEXT){
    if (args.type!=_VECT || args.subtype==_SEQ__VECT || args._VECTptr->empty())
      return gensizeerr(contextptr);
    int n=int(args._VECTptr->size());
    gen g=_rand(n,contextptr)+(abs_calc_mode(contextptr)==38?-1:0);
    if (g.type!=_INT_ || g.val<0 || g.val>=n)
      return gendimerr(contextptr);
    return args[g.val];
  }
  static const char _choice_s []="choice";
  static define_unary_function_eval (__choice,&_choice,_choice_s);
  define_unary_function_ptr5( at_choice ,alias_at_choice,&__choice,0,true);

  gen _shuffle(const gen & a,GIAC_CONTEXT){
    gen args(a);
    if (is_integral(args))
      return vector_int_2_vecteur(randperm(args.val,contextptr),contextptr);
    if (args.type!=_VECT || args._VECTptr->empty())
      return gensizeerr(contextptr);
    vecteur v(*args._VECTptr);
    int n=int(v.size());
    vecteur w(n);
    vector<int> p=randperm(n,contextptr);
    for (int i=0;i<n;++i){
      w[i]=v[p[i]];
    }
    return gen(w,args.subtype);
  }
  static const char _shuffle_s []="shuffle";
  static define_unary_function_eval (__shuffle,&_shuffle,_shuffle_s);
  define_unary_function_ptr5( at_shuffle ,alias_at_shuffle,&__shuffle,0,true);

  gen _sample(const gen & args,GIAC_CONTEXT){
    if (args.type!=_VECT || args._VECTptr->size()!=2)
      return gensizeerr(contextptr);
    gen a=args._VECTptr->front(),b=args._VECTptr->back();
    if (a.type!=_VECT || !is_integral(b) || b.type==_ZINT || b.val<0)
      return gensizeerr(contextptr);
    return _rand(makesequence(b,a),contextptr);
  }
  static const char _sample_s []="sample";
  static define_unary_function_eval (__sample,&_sample,_sample_s);
  define_unary_function_ptr5( at_sample ,alias_at_sample,&__sample,0,true);

  gen _srand(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_INT_){
      int n=args.val;
#ifdef GIAC_HAS_STO_38
      n = (1000000000*ulonglong(n))% 2147483647;
#endif
#ifndef FXCG
      srand(n);
#endif
      rand_seed(n,contextptr);
      return args;
    }
    else {
#if defined RTOS_THREADX || defined BESTA_OS
      int t=PrimeGetNow();
#else
#ifdef FXCG
      int t=RTC_GetTicks();
#else
      int t=int(time(NULL));
#endif // FXCG
#endif // RTOS/BESTA
      t = (1000000000*ulonglong(t))% 2147483647;
#ifdef VISUALC
      // srand48(t);
#endif
      rand_seed(t,contextptr);
#ifndef FXCG
      srand(t);
#endif
      return t;
    }
  }
  static const char _srand_s []="srand";
  static define_unary_function_eval (__srand,&_srand,_srand_s);
  define_unary_function_ptr5( at_srand ,alias_at_srand ,&__srand,0,T_RETURN);

  static gen symb_char(const gen & args){
    return symbolic(at_char,args);
  }
  gen _char(const gen & args_,GIAC_CONTEXT){
    if ( args_.type==_STRNG &&  args_.subtype==-1) return  args_;
    string s;
    gen args(args_);
    if (is_integral(args)){
      s += char(args.val) ;
    }
    else {
      if (args.type==_VECT){
	vecteur v=*args._VECTptr;
	iterateur it=v.begin(),itend=v.end();
	for (;it!=itend;++it){
	  if (is_integral(*it))
	    s += char(it->val);
	  else return gensizeerr(contextptr);
	}
      }
      else return gensizeerr(contextptr);
    }
    gen tmp=string2gen(s,false);
    return tmp;
  }
  static const char _char_s []="char";
  static define_unary_function_eval (__char,&_char,_char_s);
  define_unary_function_ptr5( at_char ,alias_at_char,&__char,0,true);

  static const char _chr_s []="chr";
  static define_unary_function_eval (__chr,&_char,_chr_s);
  define_unary_function_ptr5( at_chr ,alias_at_chr,&__chr,0,true);

  static gen symb_asc(const gen & args){
    return symbolic(at_asc,args);
  }
  gen _asc(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_STRNG){
      int l=int(args._STRNGptr->size());
      vecteur v(l);
      for (int i=0;i<l;++i)
	v[i]=int( (unsigned char) ((*args._STRNGptr)[i]));
      // if (abs_calc_mode(contextptr)==38) return gen(v,_LIST__VECT);
      return v;
    }
    if (args.type==_VECT){
      if ( (args._VECTptr->size()!=2) ||(args._VECTptr->front().type!=_STRNG) || (args._VECTptr->back().type!=_INT_) )
	return gensizeerr(gettext("asc"));
      return int( (unsigned char) (*args._VECTptr->front()._STRNGptr)[args._VECTptr->back().val]);
    }
    else return symb_asc(args);
  }

  static const char _asc_s []="asc";
  static define_unary_function_eval (__asc,&_asc,_asc_s);
  define_unary_function_ptr5( at_asc ,alias_at_asc,&__asc,0,true);

  static gen symb_map(const gen & args){
    return symbolic(at_map,args);
  }
  gen _map(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symb_map(args);
    vecteur v=*args._VECTptr;
    int s=int(v.size());
    if (s<2)
      return gentoofewargs("");
    gen objet=v.front();
    gen to_map=v[1];
    // FIXME: should have maple_map and mupad_map functions
    if (xcas_mode(contextptr)==1 || (to_map.type!=_FUNC && !to_map.is_symb_of_sommet(at_program) && (objet.type==_FUNC || objet.is_symb_of_sommet(at_program)))){
      objet=v[1];
      to_map=v.front();
    }
    bool matrix = ckmatrix(objet) && s>2;
    if (matrix){
      matrix=false;
      for (int i=2;i<s;++i){
	if (v[i]==at_matrix){
	  v.erase(v.begin()+i);
	  --s;
	  matrix=true;
	  break;
	}
      }
    }
    if (to_map.type==_VECT)
      return gensizeerr(contextptr);
    if (v.size()==2){
      if (objet.type==_SYMB){
	gen & f=objet._SYMBptr->feuille;
	gen tmp;
	if (xcas_mode(contextptr)==1)
	  tmp=_map(makevecteur(to_map,f),contextptr);
	else
	  tmp=_map(makevecteur(f,to_map),contextptr);
	if (f.type==_VECT && tmp.type==_VECT)
	  tmp.subtype=f.subtype;
	if (is_equal(objet) || objet._SYMBptr->sommet==at_same)
	  return symbolic(at_equal,tmp);
	return objet._SYMBptr->sommet(tmp,contextptr);
      }
      // if (to_map.type==_FUNC) return apply(objet,*to_map._FUNCptr);
      if (objet.type==_POLY){
	int dim=objet._POLYptr->dim;
	polynome res(dim);
	vector< monomial<gen> >::const_iterator it=objet._POLYptr->coord.begin(),itend=objet._POLYptr->coord.end();
	res.coord.reserve(itend-it);
	vecteur argv(dim+1);
	for (;it!=itend;++it){
	  argv[0]=it->value;
	  index_t::const_iterator i=it->index.begin();
	  for (int j=0;j<dim;++j,++i)
	    argv[j+1]=*i;
	  gen g=to_map(gen(argv,_SEQ__VECT),contextptr);
	  if (!is_zero(g))
	    res.coord.push_back(monomial<gen>(g,it->index));
	}
	return res;
      }
      if (objet.type!=_VECT)
	return to_map(objet,contextptr);
      const_iterateur it=objet._VECTptr->begin(),itend=objet._VECTptr->end();
      vecteur res;
      res.reserve(itend-it);
      for (;it!=itend;++it){
	if (matrix && it->type==_VECT){
	  const vecteur & tmp = *it->_VECTptr;
	  const_iterateur jt=tmp.begin(),jtend=tmp.end();
	  vecteur tmpres;
	  tmpres.reserve(jtend-jt);
	  for (;jt!=jtend;++jt){
	    tmpres.push_back(to_map(*jt,contextptr));
	  }
	  res.push_back(tmpres);
	}
	else
	  res.push_back(to_map(*it,contextptr));
      }
      return res;
    }
    if (objet.type==_POLY){
      int dim=objet._POLYptr->dim;
      vecteur opt(v.begin()+2,v.end());
      opt=mergevecteur(vecteur(dim+1),opt);
      polynome res(dim);
      vector< monomial<gen> >::const_iterator it=objet._POLYptr->coord.begin(),itend=objet._POLYptr->coord.end();
      res.coord.reserve(itend-it);
      for (;it!=itend;++it){
	opt[0]=it->value;
	index_t::const_iterator i=it->index.begin();
	for (int j=0;j<dim;++j,++i)
	  opt[j+1]=*i;
	gen g=to_map(gen(opt,_SEQ__VECT),contextptr);
	if (!is_zero(g))
	  res.coord.push_back(monomial<gen>(g,it->index));
      }
      return res;
    }
    vecteur opt(v.begin()+1,v.end());
    opt[0]=objet;
    if (objet.type!=_VECT)
      return to_map(opt,contextptr);
    const_iterateur it=objet._VECTptr->begin(),itend=objet._VECTptr->end();
    vecteur res;
    res.reserve(itend-it);
    for (;it!=itend;++it){
      if (matrix && it->type==_VECT){
	const vecteur & tmp = *it->_VECTptr;
	const_iterateur jt=tmp.begin(),jtend=tmp.end();
	vecteur tmpres;
	tmpres.reserve(jtend-jt);
	for (;jt!=jtend;++jt){
	  opt[0]=*jt;
	  tmpres.push_back(to_map(gen(opt,_SEQ__VECT),contextptr));
	}
	res.push_back(tmpres);
      }
      else {
	opt[0]=*it;
	res.push_back(to_map(gen(opt,_SEQ__VECT),contextptr));
      }
    }
    return res;
  }
  static const char _map_s []="map";
  static define_unary_function_eval (__map,&_map,_map_s);
  define_unary_function_ptr5( at_map ,alias_at_map,&__map,0,true);
  
  static gen symb_apply(const gen & args){
    return symbolic(at_apply,args);
  }
  gen _apply(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symb_apply(args);
    if (args._VECTptr->empty())
      return gensizeerr(gettext("apply"));
    vecteur v=*args._VECTptr;
    gen to_apply=v.front();
    int n=to_apply.subtype;
    int n2=int(v.size());
    for (int i=2;i<n2;++i){
      if (v[i]==at_matrix){
	swapgen(v[0],v[1]);
	return _map(gen(v,args.subtype),contextptr);
      }
    }
    if (to_apply.type!=_FUNC)
      n=n2-1;
    int nargs=1;
    if (to_apply.is_symb_of_sommet(at_program) && to_apply._SYMBptr->feuille[0].type==_VECT)
      nargs=to_apply._SYMBptr->feuille[0]._VECTptr->size();
    if (n && (n2==n+1) ){
      vecteur res;
      for (int i=0;;++i){
	vecteur tmp;
	bool finished=true;
	for (int j=1;j<=n;++j){
	  gen & g=v[j];
	  if (g.type!=_VECT)
	    tmp.push_back(g);
	  else {
	    if (signed(g._VECTptr->size())>i){
	      finished=false;
	      tmp.push_back((*g._VECTptr)[i]);
	    }
	    else
	      tmp.push_back(zero);
	  }
	}
	if (finished)
	  break;
	if (n==1){
	  gen tmp1=tmp.front();
	  if (nargs>1 && tmp1.type==_VECT) // for apply((j,k)->j*k,matrix 2 cols)
	    tmp1.subtype=_SEQ__VECT;
	  res.push_back(to_apply(tmp1,contextptr));
	}
	else
	  res.push_back(to_apply(tmp,contextptr));
      }
      return res;
    }
    else
      return gensizeerr(contextptr);
    return 0;
  }
  static const char _apply_s []="apply";
  static define_unary_function_eval (__apply,&_apply,_apply_s);
  define_unary_function_ptr5( at_apply ,alias_at_apply,&__apply,0,true);
  
  // static gen symb_makelist(const gen & args){  return symbolic(at_makelist,args);  }
  gen _makelist(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_INT_){
      if (args.val>=0 && args.val<LIST_SIZE_LIMIT)
	return vecteur(args.val);
      if (args.val<0 && args.val>-LIST_SIZE_LIMIT){
	gen res=new_ref_vecteur(vecteur(0));
	res._VECTptr->reserve(-args.val);
	return res;
      }
    }
    if (args.type!=_VECT)
      return gensizeerr();
    vecteur v(*args._VECTptr);
    int s=int(v.size());
    if (s<2)
      return gensizeerr(contextptr);
    gen f(v[0]),debut,fin,step(1);
    if (v[1].is_symb_of_sommet(at_interval)){
      debut=v[1]._SYMBptr->feuille._VECTptr->front();
      fin=v[1]._SYMBptr->feuille._VECTptr->back();
      if (s>2)
	step=v[2];
    }
    else {
      if (s<3)
	return gensizeerr(contextptr);
      debut=v[1];
      fin=v[2];
      if (s>3)
	step=v[3];
    }
    if (is_zero(step))
      return gensizeerr(gettext("Invalid null step"));
    if (is_greater((fin-debut)/step,LIST_SIZE_LIMIT,contextptr))
      return gendimerr(contextptr);
    vecteur w;
    if (is_greater(fin,debut,contextptr)){
      step=abs(step,contextptr);
      for (gen i=debut;is_greater(fin,i,contextptr);i=i+step)
	w.push_back(f(i,contextptr));
    }
    else {
      step=-abs(step,contextptr);
      for (gen i=debut;is_greater(i,fin,contextptr);i=i+step)
	w.push_back(f(i,contextptr));
    }
    return w;
  }
  static const char _makelist_s []="makelist";
  static define_unary_function_eval (__makelist,&_makelist,_makelist_s);
  define_unary_function_ptr5( at_makelist ,alias_at_makelist,&__makelist,0,true);

  static gen symb_interval(const gen & args){
    return symbolic(at_interval,args);
  }
  gen symb_interval(const gen & a,const gen & b){
    return symbolic(at_interval,makesequence(a,b));
  }
  gen _interval(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return symb_interval(args);
  }
  static const char _interval_s []=" .. ";
  static define_unary_function_eval4_index (56,__interval,&_interval,_interval_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_interval ,alias_at_interval ,&__interval);
  
  static string printascomment(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if (feuille.type!=_STRNG)
      return string(sommetstr)+('('+feuille.print(contextptr)+')');
    string chaine=*feuille._STRNGptr;
    int s=int(chaine.size());
    if ( (xcas_mode(contextptr)==1) || (xcas_mode(contextptr)==1+_DECALAGE)){
      string res("# ");
      for (int i=0;i<s;++i){
	if ((i==s-1)||(chaine[i]!='\n'))
	  res +=chaine[i];
	else
	  res += indent(contextptr)+"# ";
      }
      return res;
    }
    int l=int(chaine.find_first_of('\n'));
    if ((l<0)|| (l>=s))
        return "//"+chaine + indent(contextptr);
    return "/*"+chaine+"*/";
  }
  static gen symb_comment(const gen & args){
    return symbolic(at_comment,args);
  }
  gen _comment(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return symb_comment(args);
  }
  static const char _comment_s []="comment";
  static define_unary_function_eval2 (__comment,&_comment,_comment_s,&printascomment);
  define_unary_function_ptr5( at_comment ,alias_at_comment ,&__comment,0,true);

  // static gen symb_throw(const gen & args){  return symbolic(at_throw,args);  }
  gen _throw(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return gensizeerr(args.print(contextptr));
  }
  static const char _throw_s []="throw";
  static define_unary_function_eval (__throw,&_throw,_throw_s);
  define_unary_function_ptr5( at_throw ,alias_at_throw ,&__throw,0,T_RETURN);

  gen symb_union(const gen & args){
    return symbolic(at_union,args);
  }
  gen _union(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return gensizeerr(contextptr);
    vecteur & v =*args._VECTptr;
    if (v.empty())
      return args;
    if (v.size()==1 && v.front().type==_VECT)
      return gen(v,_SET__VECT).eval(1,contextptr);
    if (v.size()!=2)
      return gensizeerr(contextptr);
    gen a=v.front(),b=v.back();
#if defined HAVE_LIBMPFI && !defined NO_RTTI      
    if (a.type==_REAL && b.type==_REAL){
      if (real_interval * aptr=dynamic_cast<real_interval *>(a._REALptr)){
	if (real_interval * bptr=dynamic_cast<real_interval *>(b._REALptr)){
	  mpfi_t tmp;
	  mpfi_init2(tmp,giacmin(mpfi_get_prec(aptr->infsup),mpfi_get_prec(bptr->infsup)));
	  mpfi_union(tmp,aptr->infsup,bptr->infsup);
	  gen res=real_interval(tmp);
	  mpfi_clear(tmp);
	  return res;
	}
	else {
	  if (contains(a,b))
	    return a;
	  else
	    return _union(makesequence(a,eval(gen(makevecteur(b,b),_INTERVAL__VECT),1,contextptr)),contextptr);
	}
      }
      if (contains(b,a))
	return b;
      else
	return _union(makesequence(b,eval(gen(makevecteur(a,a),_INTERVAL__VECT),1,contextptr)),contextptr);
    }
    if (a.type==_REAL){
      if (contains(a,b))
	return a;
      else
	return _union(makesequence(a,eval(gen(makevecteur(b,b),_INTERVAL__VECT),1,contextptr)),contextptr);
    }
    if (b.type==_REAL){
      if (contains(b,a))
	return b;
      else
	return _union(makesequence(a,eval(gen(makevecteur(b,b),_INTERVAL__VECT),1,contextptr)),contextptr);
    }
#endif
    if ( (a.type!=_VECT) || (b.type!=_VECT))
      return gensizeerr(gettext("Union"));
    return gen(mergevecteur(*a._VECTptr,*b._VECTptr),_SET__VECT).eval(1,contextptr);
  }
  static const char _union_s []=" union ";
  static define_unary_function_eval4_index (58,__union,&_union,_union_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_union ,alias_at_union ,&__union);

  void chk_set(gen & a){
    if (a.type==_VECT && a.subtype!=_SET__VECT){
      vecteur av=*a._VECTptr; comprim(av); a=av;
    }
  }
  gen symb_intersect(const gen & args){
    return symbolic(at_intersect,args);
  }
  gen _intersect(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if ((args.type!=_VECT) || (args._VECTptr->size()!=2))
      return gensizeerr();
    gen a=args._VECTptr->front(),b=args._VECTptr->back();
#if defined HAVE_LIBMPFI && !defined NO_RTTI      
    if (a.type==_REAL && b.type==_REAL){
      if (real_interval * aptr=dynamic_cast<real_interval *>(a._REALptr)){
	if (real_interval * bptr=dynamic_cast<real_interval *>(b._REALptr)){
	  mpfi_t tmp;
	  mpfi_init2(tmp,giacmin(mpfi_get_prec(aptr->infsup),mpfi_get_prec(bptr->infsup)));
	  mpfi_intersect(tmp,aptr->infsup,bptr->infsup);
	  gen res;
	  if (mpfi_is_empty(tmp))
	    res=vecteur(0);
	  else
	    res=real_interval(tmp);
	  mpfi_clear(tmp);
	  return res;
	}
	else {
	  if (contains(a,b))
	    return eval(gen(makevecteur(b,b),_INTERVAL__VECT),1,contextptr);
	  return vecteur(0);
	}
      }
      if (contains(b,a))
	return eval(gen(makevecteur(a,a),_INTERVAL__VECT),1,contextptr);
      return vecteur(0);
    }
    if (a.type==_REAL){
      if (contains(a,b))
	return eval(gen(makevecteur(b,b),_INTERVAL__VECT),1,contextptr);
      else
	return vecteur(0);
    }
    if (b.type==_REAL){
      if (contains(b,a))
	return a;
      else
	return vecteur(0);
    }
#endif
    if ( a.type==_VECT && b.type==_VECT){
      chk_set(a);
      chk_set(b);
      vecteur v;
      const_iterateur it=a._VECTptr->begin(),itend=a._VECTptr->end();
      for (;it!=itend;++it){
	if (equalposcomp(*b._VECTptr,*it))
	  v.push_back(*it);
      }
      return gen(v,_SET__VECT);
    }
    return gensizeerr(contextptr);
  }
  static const char _intersect_s []=" intersect ";
  static define_unary_function_eval4_index (62,__intersect,&_intersect,_intersect_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_intersect ,alias_at_intersect ,&__intersect);

  gen symb_minus(const gen & args){
    return symbolic(at_minus,args);
  }
  gen _minus(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if ((args.type!=_VECT) || (args._VECTptr->size()!=2))
      return symb_minus(args);
    gen a=args._VECTptr->front(),b=args._VECTptr->back();
    if ( (a.type!=_VECT) || (b.type!=_VECT))
      return gensizeerr(gettext("Minus"));
    chk_set(a);
    chk_set(b);
    vecteur v;
    const_iterateur it=a._VECTptr->begin(),itend=a._VECTptr->end();
    for (;it!=itend;++it){
      if (!equalposcomp(*b._VECTptr,*it))
	v.push_back(*it);
    }
    return gen(v,_SET__VECT);
  }
  static const char _minus_s []=" minus ";
  static define_unary_function_eval4_index (60,__minus,&_minus,_minus_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_minus ,alias_at_minus ,&__minus);

  static string printasdollar(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if (feuille.type!=_VECT)
      return sommetstr+feuille.print(contextptr);
    vecteur & v=*feuille._VECTptr;
    int s=int(v.size());
    if (s==2)
      return printsommetasoperator(feuille,sommetstr,contextptr);
    if (s==3)
      return v[0].print(contextptr)+sommetstr+v[1].print(contextptr)+" in "+v[2].print(contextptr);
    return gettext("error");
  }
  gen symb_dollar(const gen & args){
    return symbolic(at_dollar,args);
  }
  gen _dollar(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
#if 0
    if (abs_calc_mode(contextptr)==38){
      int r,c;
      if (args.type==_VECT && args._VECTptr->size()==2 && args._VECTptr->back().type==_INT_ && args._VECTptr->back().val>0){
	string s=args._VECTptr->front().print(contextptr)+"$"+print_INT_(args._VECTptr->back().val);
	identificateur cellule(s);
	return eval(cellule,1,contextptr);
      }
    }
#endif
    vecteur vargs;
    if (args.type!=_VECT){
      identificateur tmp(" _t");
      vargs=makevecteur(tmp,symb_equal(tmp,args)); // makevecteur(tmp,symbolic(at_equal,gen(makevecteur(tmp,args),_SEQ__VECT)));
    }
    else
      vargs=*args._VECTptr;
    int s=int(vargs.size());
    if (s<2)
      return symb_dollar(args);
    gen a=vargs.front(),b=vargs[1],b1=eval(b,eval_level(contextptr),contextptr);
    if (b1.type==_VECT && b1.subtype==_SEQ__VECT && b1._VECTptr->size()==2){
      gen b11=b1._VECTptr->front();
      if (b.type==_VECT && b.subtype==_SEQ__VECT && b._VECTptr->size()==2 && b11.is_symb_of_sommet(at_equal) && b._VECTptr->front().is_symb_of_sommet(at_equal))
	b11=symb_equal(b._VECTptr->front()[1],b11[2]);
      return _dollar(makesequence(a,b11,b1._VECTptr->back()),contextptr);
    }
    if (a.is_symb_of_sommet(at_interval) && a._SYMBptr->feuille.type==_VECT && a._SYMBptr->feuille._VECTptr->size()==2){
      a=eval(a,1,contextptr);
      gen a1=a._SYMBptr->feuille._VECTptr->front(),a2=a._SYMBptr->feuille._VECTptr->back();
      gen nstep=(a2-a1)/b1;
      if (nstep.type==_DOUBLE_)
	nstep=(1+1e-12)*nstep;
      if (ck_is_positive(nstep,contextptr))
	nstep=_floor(nstep,contextptr);
      else {
	nstep=_floor(-nstep,contextptr);
	b1=-b1;
      }
      if (nstep.type!=_INT_ || nstep.val<0 || nstep.val>LIST_SIZE_LIMIT)
	return gendimerr(contextptr);
      vecteur res;
      for (unsigned i=0;int(i)<=nstep.val;++i){
	res.push_back(a1);
	a1 += b1;
      }
      return gen(res,_SEQ__VECT);
    }
    if (b1.type==_INT_ && b1.val>=0){
      if (b1.val>LIST_SIZE_LIMIT)
	return gendimerr(contextptr);
      return gen(vecteur(b1.val,eval(a,eval_level(contextptr),contextptr)),_SEQ__VECT);
    }
    gen var,intervalle,step=1;
    if ( (b.type==_SYMB) && (is_equal(b) || b._SYMBptr->sommet==at_same ) ){
      var=b._SYMBptr->feuille._VECTptr->front();
      if (var.type!=_IDNT)
	return gensizeerr(contextptr);
      /* Commented example seq(irem(g&^((p-1)/(Div[i])),p),i=(1 .. 2))
	 int status=*var._IDNTptr->quoted;
	 *var._IDNTptr->quoted=1;
	 a=eval(a,contextptr);
	 *var._IDNTptr->quoted=status;      
      */
      intervalle=eval(b._SYMBptr->feuille._VECTptr->back(),eval_level(contextptr),contextptr);
      if (s>=3)
	step=vargs[2];
    }
    else {
      if (s>=3){
	var=vargs[1];
	intervalle=eval(vargs[2],eval_level(contextptr),contextptr);
      }
      if (s>=4)
	step=vargs[3];
    }
    if (intervalle.type==_VECT){
      const_iterateur it=intervalle._VECTptr->begin(),itend=intervalle._VECTptr->end();
      vecteur res;
      for (;it!=itend;++it)
	res.push_back(eval(quotesubst(a,var,*it,contextptr),eval_level(contextptr),contextptr));
      return gen(res,_SEQ__VECT);
      // return gen(res,intervalle.subtype);
    }
    if ( (intervalle.type==_SYMB) && (intervalle._SYMBptr->sommet==at_interval)){
      gen c=intervalle._SYMBptr->feuille._VECTptr->front(),d=intervalle._SYMBptr->feuille._VECTptr->back();
      //if ( (!is_integral(c) || !is_integral(d)) && abs_calc_mode(contextptr)==38) return gensizeerr(contextptr);
      gen debut=c,fin=d;
      bool reverse=ck_is_greater(debut,fin,contextptr);
      step=abs(step,contextptr);
      if (is_positive(abs(fin-debut,contextptr)-LIST_SIZE_LIMIT*step,contextptr))
	return gendimerr(contextptr);
      step=eval(reverse?-step:step,eval_level(contextptr),contextptr);
      vecteur res;
      for (;;debut+=step){
	if (ck_is_strictly_greater(reverse?fin:debut,reverse?debut:fin,contextptr))
	  break;
	res.push_back(eval(quotesubst(a,var,debut,contextptr),eval_level(contextptr),contextptr));
	if (debut==fin)
	  break;
      }
      return gen(res,_SEQ__VECT);
    }
    return symb_dollar(args);    
  }
  static const char _dollar_s []="$";
  static define_unary_function_eval4_index (125,__dollar,&_dollar,_dollar_s,&printasdollar,&texprintsommetasoperator);
  define_unary_function_ptr5( at_dollar ,alias_at_dollar,&__dollar,_QUOTE_ARGUMENTS,0);

  static gen symb_makemat(const gen & args){
    return symbolic(at_makemat,args);
  }
  gen _makemat(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symb_makemat(args);
    int s=int(args._VECTptr->size());
    if ( (s!=3) && (s!=2) )
      return symb_makemat(args);
    gen fonction,intervalle1,intervalle2;
    if (s==3){
      fonction=args._VECTptr->front();
      intervalle1=(*(args._VECTptr))[1];
      intervalle2=args._VECTptr->back();
    }
    else {
      intervalle1=args._VECTptr->front();
      intervalle2=args._VECTptr->back();
    }
    if (is_integral(intervalle1) && intervalle1.type==_INT_)
      intervalle1=symb_interval(makevecteur(zero,giacmax(intervalle1.val,1)-1));
    if (is_integral(intervalle2) && intervalle2.type==_INT_)
      intervalle2=symb_interval(makevecteur(zero,giacmax(intervalle2.val,1)-1));
    if ( (intervalle1.type!=_SYMB) || (intervalle1._SYMBptr->sommet!=at_interval) ||(intervalle2.type!=_SYMB) || (intervalle2._SYMBptr->sommet!=at_interval))
      return gensizeerr(gettext("makemat"));
    intervalle1=intervalle1._SYMBptr->feuille;
    intervalle2=intervalle2._SYMBptr->feuille;
    if ((intervalle1.type!=_VECT) || (intervalle1._VECTptr->size()!=2) || (intervalle2.type!=_VECT) || (intervalle2._VECTptr->size()!=2))
      return gensizeerr(gettext("interval"));
    gen debut_i=intervalle1._VECTptr->front(),fin_i=intervalle1._VECTptr->back();
    gen debut_j=intervalle2._VECTptr->front(),fin_j=intervalle2._VECTptr->back();
    if ( (debut_i.type!=_INT_) || (fin_i.type!=_INT_) || (debut_j.type!=_INT_) || (fin_j.type!=_INT_) )
      return gensizeerr(gettext("Boundaries not integer"));
    int di=debut_i.val,fi=fin_i.val,dj=debut_j.val,fj=fin_j.val;
    if (array_start(contextptr)){
      ++di; ++fi; ++dj; ++fj;
    }
    int stepi=1,stepj=1;
    if (di>fi)
      stepi=-1;
    if (dj>fj)
      stepj=-1;
    if ((fonction.type!=_SYMB) || (fonction._SYMBptr->sommet!=at_program)){
      int s=(fj-dj+1)*stepj;
      vecteur w(s,fonction);
      int t=(fi-di+1)*stepi;
      vecteur res(t);
      for (int i=0;i<t;++i)
	res[i]=w; // each element of res will be a free line, so that =< works
      return gen(res,_MATRIX__VECT);
    }
    vecteur v,w,a(2);
    v.reserve((fi-di)*stepi);
    w.reserve((fj-dj)*stepj);
    for (;;di+=stepi){
      a[0]=di;
      w.clear();
      for (int djj=dj;;djj+=stepj){
	a[1]=djj;
	w.push_back(fonction(gen(a,_SEQ__VECT),contextptr));
	if (djj==fj)
	  break;
      }
      v.push_back(w);
      if (di==fi)
	break;
    }
    return gen(v,_MATRIX__VECT);
  }
  static const char _makemat_s []="makemat";
  static define_unary_function_eval (__makemat,&_makemat,_makemat_s);
  define_unary_function_ptr5( at_makemat ,alias_at_makemat,&__makemat,0,true);

  gen symb_compose(const gen & args){
    return symbolic(at_compose,args);
  }
  gen _compose(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_VECT && args.subtype==_SEQ__VECT && args._VECTptr->size()==2&& args._VECTptr->back().type==_SPOL1){
      gen g=args._VECTptr->front();
      gen s=args._VECTptr->back();
      sparse_poly1 res;
      int shift=0;
      if (g.type==_SPOL1){
	vecteur v;
	if (sparse_poly12vecteur(*g._SPOL1ptr,v,shift))
	  g=v;
      }
      if (g.type==_VECT){
	vecteur v=*g._VECTptr;
	vreverse(v.begin(),v.end());
	if (!pcompose(v,*s._SPOL1ptr,res,contextptr))
	  return sparse_poly1(1,monome(1,undef));
	if (shift==0)
	  return res;
	return spmul(res,sppow(*s._SPOL1ptr,shift,contextptr),contextptr);
      }
    }
    if (args.type==_VECT && args.subtype==_SEQ__VECT && args._VECTptr->size()==2){
      gen g=args._VECTptr->front();
      gen s=args._VECTptr->back();
      if (g.type==_VECT && (g.subtype==_POLY1__VECT || s.type==_SPOL1 || (s.type==_VECT && s.subtype==_POLY1__VECT)) )
	return horner(*g._VECTptr,s,contextptr);
      if (ckmatrix(g) && ckmatrix(s))
	return g*s;
    }
    return symb_compose(args);
  }
  static const char _compose_s []="@";
  static define_unary_function_eval4 (__compose,&_compose,_compose_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_compose ,alias_at_compose ,&__compose);

  gen _composepow(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_VECT && args.subtype==_SEQ__VECT && args._VECTptr->size()==2){
      gen base=args._VECTptr->front();
      gen expo=args._VECTptr->back();
      if (expo.type==_INT_){
	if (base.type==_VECT && base.subtype==_POLY1__VECT && expo.val>=0){
	  int n=expo.val;
	  vecteur v(2); v[0]=1;
	  gen res=gen(v,_POLY1__VECT);
	  gen s=base;
	  for (;n>0;n/=2){
	    if (n%2) res=horner(*res._VECTptr,s,contextptr);
	    s=horner(*s._VECTptr,s,contextptr);
	  }
	  return res;
	}
	if (ckmatrix(base))
	  return _pow(args,contextptr);
      }
    }
    return symbolic(at_composepow,args);
  }
  static const char _composepow_s []="@@";
  static define_unary_function_eval4 (__composepow,&_composepow,_composepow_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_composepow ,alias_at_composepow ,&__composepow);

  gen symb_args(const gen & args){
    return symbolic(at_args,args);
  }
  gen _args(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    gen e;
    if (debug_ptr(contextptr)->args_stack.empty())
      e=vecteur(0);
    else
      e=debug_ptr(contextptr)->args_stack.back();
    if ( (args.type==_VECT) && (args._VECTptr->empty()))
      return e;
    else
      return e(args,contextptr);
  }
  static const char _args_s []="args";
  static define_unary_function_eval (__args,&_args,_args_s);
  define_unary_function_ptr( at_args ,alias_at_args ,&__args);
  
  // static gen symb_lname(const gen & args){  return symbolic(at_lname,args);  }
  static void lidnt(const vecteur & v,vecteur & res,bool with_at){
    const_iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it)
      lidnt(*it,res,with_at);
  }

  extern const unary_function_ptr * const  at_int;

  void lidnt(const gen & args,vecteur & res,bool with_at){
    switch (args.type){
    case _IDNT:
      if (!equalposcomp(res,args))
	res.push_back(args);
      break;
    case _SYMB:
      if (with_at && args._SYMBptr->sommet==at_at){
	res.push_back(args);
	return;
      }
      if (args._SYMBptr->sommet==at_program && args._SYMBptr->feuille.type==_VECT && args._SYMBptr->feuille._VECTptr->size()==3){
	lidnt(args._SYMBptr->feuille._VECTptr->front(),res,with_at);
	lidnt(args._SYMBptr->feuille._VECTptr->back(),res,with_at);
	return;
      }
      if (args._SYMBptr->sommet==at_pnt && args._SYMBptr->feuille.type==_VECT && args._SYMBptr->feuille._VECTptr->size()==3){
	lidnt(args._SYMBptr->feuille._VECTptr->front(),res,with_at);
	lidnt((*args._SYMBptr->feuille._VECTptr)[1],res,with_at);
	return;
      }
      if ( (args._SYMBptr->sommet==at_integrate || args._SYMBptr->sommet==at_int || args._SYMBptr->sommet==at_sum) && args._SYMBptr->feuille.type==_VECT && args._SYMBptr->feuille._VECTptr->size()==4){
	vecteur & v =*args._SYMBptr->feuille._VECTptr;
	vecteur w(1,v[1]);
	lidnt(v[0],w,with_at);
	const_iterateur it=w.begin(),itend=w.end();
	for (++it;it!=itend;++it)
	  lidnt(*it,res,with_at);
	lidnt(v[2],res,with_at);
	lidnt(v.back(),res,with_at);
	return;
      }      
      lidnt(args._SYMBptr->feuille,res,with_at);
      break;
    case _VECT:
      lidnt(*args._VECTptr,res,with_at);
      break;
    }       
  }
  vecteur lidnt(const gen & args){
    vecteur res;
    lidnt(args,res,false);
    return res;
  }
  vecteur lidnt_with_at(const gen & args){
    vecteur res;
    lidnt(args,res,true);
    return res;
  }
  gen _lname(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    vecteur res=makevecteur(cst_pi,cst_euler_gamma);
    lidnt(args,res,false);
    return vecteur(res.begin()+2,res.end());
  }
  static const char _lname_s []="lname";
  static define_unary_function_eval (__lname,&_lname,_lname_s);
  define_unary_function_ptr5( at_lname ,alias_at_lname,&__lname,0,true);

  static gen symb_has(const gen & args){
    return symbolic(at_has,args);
  }
  gen _has(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if ( (args.type!=_VECT) || (args._VECTptr->size()!=2))
      return symb_has(args);
    gen tmp(_lname(args._VECTptr->front(),contextptr));
    if (tmp.type!=_VECT) return tmp;
    return equalposcomp(*tmp._VECTptr,args._VECTptr->back());
  }
  static const char _has_s []="has";
  static define_unary_function_eval (__has,&_has,_has_s);
  define_unary_function_ptr5( at_has ,alias_at_has,&__has,0,true);

  gen _kill(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_VECT && args._VECTptr->empty()){
      if (!contextptr)
	protection_level=0;
      debug_ptr(contextptr)->debug_mode=false;
      debug_ptr(contextptr)->current_instruction_stack.clear();
      debug_ptr(contextptr)->sst_at_stack.clear();
      debug_ptr(contextptr)->args_stack.clear();
      return gensizeerr(gettext("Program killed"));
    }
#ifdef HAVE_LIBPTHREAD
    if (args.type==_VECT)
      return apply(args,_kill,contextptr);
    if (args.type==_POINTER_ && args.subtype==_THREAD_POINTER){
      context * cptr=(context *) args._POINTER_val;
      thread_param * tptr =thread_param_ptr(cptr);
      if (cptr 
#ifndef __MINGW_H
	  && tptr->eval_thread
#endif
	  ){
	gen g=tptr->v[0];
	if (g.type==_VECT && g._VECTptr->size()==2 && g._VECTptr->front().is_symb_of_sommet(at_quote)){
	  pthread_mutex_lock(cptr->globalptr->_mutex_eval_status_ptr);
	  gen tmpsto=sto(undef,g._VECTptr->front()._SYMBptr->feuille,cptr);
	  if (is_undef(tmpsto)) return tmpsto;
	  pthread_mutex_unlock(cptr->globalptr->_mutex_eval_status_ptr);
	}
      }
      kill_thread(true,cptr);
      return 1;
    }
#endif
    return gentypeerr(contextptr);
  }
  static const char _kill_s []="kill";
  static define_unary_function_eval (__kill,&_kill,_kill_s);
  define_unary_function_ptr5( at_kill ,alias_at_kill,&__kill,0,true);

  gen _halt(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (debug_ptr(contextptr)->debug_allowed){
      debug_ptr(contextptr)->debug_mode=true;
      debug_ptr(contextptr)->sst_mode=true;
      return plus_one;
    }
    return zero;
  }
  static const char _halt_s []="halt";
  static define_unary_function_eval_quoted (__halt,&_halt,_halt_s);
  define_unary_function_ptr5( at_halt ,alias_at_halt,&__halt,_QUOTE_ARGUMENTS,true);

  gen _debug(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (child_id && thread_eval_status(contextptr)!=1)
      return args;
    if (debug_ptr(contextptr)->debug_allowed){
      debug_ptr(contextptr)->debug_mode=true;
      debug_ptr(contextptr)->sst_in_mode=true;
      debug_ptr(contextptr)->debug_prog_name=0;
    }
    return args.eval(eval_level(contextptr),contextptr);
  }
  static const char _debug_s []="debug";
  static define_unary_function_eval_quoted (__debug,&_debug,_debug_s);
  define_unary_function_ptr5( at_debug ,alias_at_debug,&__debug,_QUOTE_ARGUMENTS,true);

  gen _sst_in(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (child_id)
      return zero;
    if (debug_ptr(contextptr)->debug_allowed){
      debug_ptr(contextptr)->debug_mode=true;
      debug_ptr(contextptr)->sst_in_mode=true;
      return plus_one;
    }
    return zero;
  }
  static const char _sst_in_s []="sst_in";
  static define_unary_function_eval_quoted (__sst_in,&_sst_in,_sst_in_s);
  define_unary_function_ptr5( at_sst_in ,alias_at_sst_in,&__sst_in,_QUOTE_ARGUMENTS,true);

  gen _sst(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (child_id)
      return args;
    if (debug_ptr(contextptr)->debug_allowed){
      debug_ptr(contextptr)->debug_mode=true;
      debug_ptr(contextptr)->sst_mode=true;
      return plus_one;
    }
    return zero;
  }
  static const char _sst_s []="sst";
  static define_unary_function_eval_quoted (__sst,&_sst,_sst_s);
  define_unary_function_ptr5( at_sst ,alias_at_sst,&__sst,_QUOTE_ARGUMENTS,true);

  gen _cont(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (child_id)
      return args;
    if (debug_ptr(contextptr)->debug_allowed){
      debug_ptr(contextptr)->sst_mode=false;
      return plus_one;
    }
    return zero;
  }
  static const char _cont_s []="cont";
  static define_unary_function_eval_quoted (__cont,&_cont,_cont_s);
  define_unary_function_ptr5( at_cont ,alias_at_cont,&__cont,_QUOTE_ARGUMENTS,true);

  static gen watch(const gen & args,GIAC_CONTEXT){
    if (!equalposcomp(debug_ptr(contextptr)->debug_watch,args))
      debug_ptr(contextptr)->debug_watch.push_back(args);
    return args;
  }
  gen _watch(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (child_id && thread_eval_status(contextptr)!=1 )
      return args;
    if (args.type==_VECT && args._VECTptr->empty() && debug_ptr(contextptr)->debug_localvars)
      apply( *debug_ptr(contextptr)->debug_localvars,contextptr,watch);
    else
      apply(args,contextptr,watch);
    return debug_ptr(contextptr)->debug_watch;
  }
  static const char _watch_s []="watch";
  static define_unary_function_eval_quoted (__watch,&_watch,_watch_s);
  define_unary_function_ptr5( at_watch ,alias_at_watch,&__watch,_QUOTE_ARGUMENTS,true);

  static gen rmwatch(const gen & args,GIAC_CONTEXT){
    int pos;
    if (args.type==_INT_){
      pos=args.val+1;
      if (pos>signed(debug_ptr(contextptr)->debug_watch.size()))
	return debug_ptr(contextptr)->debug_watch;
    }
    else 
      pos=equalposcomp(debug_ptr(contextptr)->debug_watch,args);
    if (pos){
      debug_ptr(contextptr)->debug_watch.erase(debug_ptr(contextptr)->debug_watch.begin()+pos-1,debug_ptr(contextptr)->debug_watch.begin()+pos);
      return debug_ptr(contextptr)->debug_watch;
    }
    return zero;
  }

  gen _rmwatch(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_VECT && args._VECTptr->empty() && debug_ptr(contextptr)->debug_localvars)
      return apply( *debug_ptr(contextptr)->debug_localvars,contextptr,rmwatch);
    else
      return apply(args,contextptr,rmwatch);
  }
  static const char _rmwatch_s []="rmwatch";
  static define_unary_function_eval_quoted (__rmwatch,&_rmwatch,_rmwatch_s);
  define_unary_function_ptr5( at_rmwatch ,alias_at_rmwatch,&__rmwatch,_QUOTE_ARGUMENTS,true);

  gen _breakpoint(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (child_id && thread_eval_status(contextptr)!=1)
      return args;
    if ( (args.type!=_VECT) || (args._VECTptr->size()!=2) || (args._VECTptr->front().type!=_IDNT) || (args._VECTptr->back().type!=_INT_) )
      return zero;
    if (!equalposcomp(debug_ptr(contextptr)->debug_breakpoint,args)){
      debug_ptr(contextptr)->debug_breakpoint.push_back(args);
      // FIXME should also modify debug_ptr(contextptr)->sst_at_stack if the breakpoint applies
      // to a program != current program
      if (!debug_ptr(contextptr)->args_stack.empty() && debug_ptr(contextptr)->args_stack.back().type==_VECT && debug_ptr(contextptr)->args_stack.back()._VECTptr->front()==args._VECTptr->front())
	debug_ptr(contextptr)->sst_at.push_back(args._VECTptr->back().val);
    }
    return debug_ptr(contextptr)->debug_breakpoint;
  }
  static const char _breakpoint_s []="breakpoint";
  static define_unary_function_eval_quoted (__breakpoint,&_breakpoint,_breakpoint_s);
  define_unary_function_ptr5( at_breakpoint ,alias_at_breakpoint,&__breakpoint,_QUOTE_ARGUMENTS,true);

  gen _rmbreakpoint(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (child_id&& thread_eval_status(contextptr)!=1)
      return args;
    int pos;
    if (args.type==_INT_){
      pos=args.val;
      if (pos<1 || pos>signed(debug_ptr(contextptr)->debug_breakpoint.size())){
	adjust_sst_at(*debug_ptr(contextptr)->debug_prog_name,contextptr);
	return debug_ptr(contextptr)->debug_breakpoint;
      }
    }
    else 
      pos=equalposcomp(debug_ptr(contextptr)->debug_breakpoint,args);
    if (pos){
      debug_ptr(contextptr)->debug_breakpoint.erase(debug_ptr(contextptr)->debug_breakpoint.begin()+pos-1,debug_ptr(contextptr)->debug_breakpoint.begin()+pos);
      adjust_sst_at(*debug_ptr(contextptr)->debug_prog_name,contextptr);
      return debug_ptr(contextptr)->debug_breakpoint;
    }
    return zero;
  }
  static const char _rmbreakpoint_s []="rmbreakpoint";
  static define_unary_function_eval_quoted (__rmbreakpoint,&_rmbreakpoint,_rmbreakpoint_s);
  define_unary_function_ptr5( at_rmbreakpoint ,alias_at_rmbreakpoint,&__rmbreakpoint,_QUOTE_ARGUMENTS,true);

#if 1
  void debug_loop(gen &res,GIAC_CONTEXT){
    if (!debug_ptr(contextptr)->debug_allowed || (!debug_ptr(contextptr)->sst_mode && !equalposcomp(debug_ptr(contextptr)->sst_at,debug_ptr(contextptr)->current_instruction)) )
      return;
    // Fill dbgptr->debug_info_ptr and fast_debug_info_ptr 
    // with debugging infos to be displayed
    debug_struct * dbgptr=debug_ptr(contextptr);
    vecteur w;
    string progs;
    // w[0]=function, args,
    // w[1]=breakpoints
    // w[2] = instruction to eval or program if debugging a prog
    // w[3]= evaluation result
    // w[4]= current instruction number 
    // w[5] = watch vector, w[6] = watch values
    if (!debug_ptr(contextptr)->args_stack.empty()){
      w.push_back(debug_ptr(contextptr)->args_stack.back());
      w.push_back(vector_int_2_vecteur(debug_ptr(contextptr)->sst_at,contextptr));
    }
    else {
      w.push_back(undef);
      w.push_back(undef);
    }
    gen w2=(*debug_ptr(contextptr)->fast_debug_info_ptr);
    if (w2.type==_VECT && w2._VECTptr->size()>3)
      w2=w2[2];
    //*logptr(contextptr) << w2 << endl;
    w.push_back(w2);
    w.push_back(res);
    w.push_back(debug_ptr(contextptr)->current_instruction);
    vecteur dw=debug_ptr(contextptr)->debug_watch;
    if (contextptr && dw.empty()){
      // put the last 2 environments
      const context * cur=contextptr;
      sym_tab::const_iterator it=cur->tabptr->begin(),itend=cur->tabptr->end();
      for (;it!=itend;++it){
	dw.push_back(identificateur(it->first));
      }
      if (cur->previous && cur->previous!=cur->globalcontextptr){
	cur=cur->previous;
	sym_tab::const_iterator it=cur->tabptr->begin(),itend=cur->tabptr->end();
	for (;it!=itend;++it){
	  dw.push_back(identificateur(it->first));
	}
      }
    }
    w.push_back(dw);
    //!!!!!!!!!!!!!!!!!!!!!! //
    Bdisp_AllClr_VRAM();
    bool save_freeze=freeze;
    freeze=true;
    int dispx=0,dispy=0;
    // print debugged program instructions from current-2 to current+3
    progs="debug "+w[0].print(contextptr)+'\n';
    if (w[4].type==_INT_){
      vector<string> ws;
#if 1
      ws.push_back("");
      debug_print(w[2],ws,contextptr);
#else
      int l=s.size();
      string cur;
      for (int i=0;i<l;++i){
	if (s[i]=='\n'){
	  ws.push_back(cur);
	  cur="";
	}
	else cur+=s[i];
      }
      ws.push_back(cur);
#endif
      int m=giacmax(0,w[4].val-2),M=giacmin(w[4].val+3,ws.size()-1);
      if (M-m<5)
	M=giacmin(m+5,ws.size()-1);
      if (M-m<5)
	m=giacmax(0,M-5);
      for (int i=m;i<=M;++i){
	//!!!!!!!!!!!!!!!!!!!!!! //
  PrintMini(dispx,dispy,(const unsigned char *)(print_INT_(i)+":"+ws[i]).c_str(),(i==w[4].val?MINI_REV:0));
	//mPrintXY(dispx,dispy,(print_INT_(i)+":"+ws[i]).c_str(),(i==w[4].val?TEXT_MODE_INVERT:TEXT_MODE_TRANSPARENT_BACKGROUND),TEXT_COLOR_BLACK);
	dispy+=12; //!!!! 6
	dispx=0;
	// progs += print_INT_(i)+((i==w[4].val)?" => ":"    ")+ws[i]+'\n';
      }
    }
    else {
      string s=w[2].print(contextptr);
      progs += "\nprg: "+s+" # "+w[4].print(contextptr);
    }
    //!!!!!!!!!!!!!!!!!!!!!! //
    draw_line(0,dispy,LCD_WIDTH_PX,dispy,COLOR_BLACK);
    dispx = 0;
    dispy += 1;
    // progs += "======\n";
    // evaluate watch with debug_ptr(contextptr)->debug_allowed=false
    debug_ptr(contextptr)->debug_allowed=false;
    string evals,breaks;
    iterateur it=dw.begin(),itend=dw.end();
    int nvars=itend-it;
    bool fewvars=nvars<4;
    for (int nv=0;it!=itend;++nv,++it){
      evals += it->print(contextptr)+"=";
      gen tmp=protecteval(*it,1,contextptr);
      string s=tmp.print(contextptr);
      if (!fewvars && s.size()>17)
	s=s.substr(0,14)+"...";
      evals += s+",";
      if (fewvars || (nv % 2)==1 || nv==nvars-1){
	//!!!!!!!!!!!!!!!!!!!!!! //
  PrintMini(dispx,dispy,(const unsigned char *)evals.c_str(),0);
	dispy+=13; //!!!! 7
	dispx=0;	
	evals="";
	// evals += '\n';
      }
      else
	evals += " | ";
    }
    if (!evals.size()==0){
      //!!!!!!!!!!!!!!!!!!!!!! //
      PrintMini(dispx,dispy,(const unsigned char *)evals.c_str(),0);
      dispx=0;
    }
    //!!!!!!!!!!!!!!!!!!!!!! //
    dispy=LCD_HEIGHT_PX-13; //!!! -7
    //!!!!!!!!!!!!!!!!!!!!!! //
    PrintMini(dispx,dispy,(const unsigned char *)"next | in  |cont |kill |     |     ",MINI_REV);  //!!!!   
    w.push_back(dw);
    debug_ptr(contextptr)->debug_allowed=true;
    *dbgptr->debug_info_ptr=w;
    while (1){
      clear_abort();
      unsigned int key;
      //!!!!!!!!!!!!!!!!!!!!!! //
      GetKey((int *)&key);
      set_abort();
      // convert key to i
      int i=0;
      //!!!!!!!!!!!!!!!!!!!!!! //
      
      if (key==KEY_CTRL_F1 || key==KEY_CTRL_EXE || key==KEY_CTRL_DOWN){
	i=-1;
      }
      if (key==KEY_CTRL_F2 || key==KEY_CTRL_RIGHT){
	i=-2;
      }
      if (key==KEY_CTRL_F3){
	i=-3;
      }
      if (key==KEY_CTRL_F4 || key==KEY_CTRL_AC){
	i=-4;
      }
      if (key==KEY_CTRL_F5){
	i=-5;
      }
      if (key==KEY_CTRL_F6){
	i=-6;
      }
      
      if (i>0){
	gen tmp=i;
	if (tmp.is_symb_of_sommet(at_equal))
	  tmp=equaltosto(tmp,contextptr);
	evals=string("eval: ")+tmp.print(contextptr)+" => "+protecteval(tmp,1,contextptr).print(contextptr)+'\n';
	std::cout << evals ;
	iterateur it=dw.begin(),itend=dw.end();
	for (;it!=itend;++it){
	  evals += it->print(contextptr)+"=";
	  gen tmp=protecteval(*it,1,contextptr);
	  evals += tmp.print(contextptr)+",";
	}
      }
      // CERR << i << endl;
      if (i==-1){
	dbgptr->sst_in_mode=false;
	dbgptr->sst_mode=true;
	freeze=save_freeze;
	return;
      }
      if (i==-2){
	dbgptr->sst_in_mode=true;
	dbgptr->sst_mode=true;
	freeze=save_freeze;
	return;
      }
      if (i==-3){
	dbgptr->sst_in_mode=false;
	dbgptr->sst_mode=false;
	freeze=save_freeze;
	return;
      }
      if (i==-4){
	dbgptr->sst_in_mode=false;
	dbgptr->sst_mode=false;
	//debug_ptr(contextptr)->current_instruction_stack.clear();
	//debug_ptr(contextptr)->sst_at_stack.clear();
	//debug_ptr(contextptr)->args_stack.clear();
	ctrl_c=interrupted=true;
	freeze=save_freeze;
	return;
      }
#if 0
      if (i==-5){
	breaks="break line "+print_INT_(w[4].val)+'\n';
	_breakpoint(makesequence(w[0][0],w[4]),contextptr);
      }
      if (i==-6){
	breaks="remove break line "+print_INT_(w[4].val)+'\n';
	_rmbreakpoint(makesequence(w[0][0],w[4]),contextptr);
      }
#endif
    } // end while(1)
    freeze=save_freeze;
   }
#else // EMCC

#ifdef GIAC_HAS_STO_38
  void aspen_debug_loop(gen & res,GIAC_CONTEXT);

  void debug_loop(gen &res,GIAC_CONTEXT){
    aspen_debug_loop(res,contextptr);
  }

#else // GIAC_HAS_STO_38

  void debug_loop(gen &res,GIAC_CONTEXT){
    if (!debug_ptr(contextptr)->debug_allowed || (!debug_ptr(contextptr)->sst_mode && !equalposcomp(debug_ptr(contextptr)->sst_at,debug_ptr(contextptr)->current_instruction)) )
      return;
    // Detect thread debugging
    int thread_debug=thread_eval_status(contextptr);
    if (thread_debug>1)
      return;
    if (thread_debug==1){
      // Fill dbgptr->debug_info_ptr and fast_debug_info_ptr 
      // with debugging infos to be displayed
      debug_struct * dbgptr=debug_ptr(contextptr);
      vecteur w; 
      // w[0]=function, args,
      // w[1]=breakpoints
      // w[2] = instruction to eval or program if debugging a prog
      // w[3]= evaluation result
      // w[4]= current instruction number 
      // w[5] = watch vector, w[6] = watch values
      if (!debug_ptr(contextptr)->args_stack.empty()){
	w.push_back(debug_ptr(contextptr)->args_stack.back());
	w.push_back(vector_int_2_vecteur(debug_ptr(contextptr)->sst_at,contextptr));
      }
      else {
	w.push_back(undef);
	w.push_back(undef);
      }
      w.push_back((*debug_ptr(contextptr)->fast_debug_info_ptr));
      w.push_back(res);
      w.push_back(debug_ptr(contextptr)->current_instruction);
      vecteur dw=debug_ptr(contextptr)->debug_watch;
      if (contextptr && dw.empty()){
	// put the last 2 environments
	const context * cur=contextptr;
	sym_tab::const_iterator it=cur->tabptr->begin(),itend=cur->tabptr->end();
	for (;it!=itend;++it){
	  dw.push_back(identificateur(it->first));
	}
	if (cur->previous && cur->previous!=cur->globalcontextptr){
	  cur=cur->previous;
	  sym_tab::const_iterator it=cur->tabptr->begin(),itend=cur->tabptr->end();
	  for (;it!=itend;++it){
	    dw.push_back(identificateur(it->first));
	  }
	}
      }
      w.push_back(dw);
      // evaluate watch with debug_ptr(contextptr)->debug_allowed=false
      debug_ptr(contextptr)->debug_allowed=false;
      iterateur it=dw.begin(),itend=dw.end();
      for (;it!=itend;++it)
	*it=protecteval(*it,1,contextptr);
      w.push_back(dw);
      debug_ptr(contextptr)->debug_allowed=true;
      *dbgptr->debug_info_ptr=w;
      dbgptr->debug_refresh=false;
      // Switch to level 2, waiting for main
      thread_eval_status(2,contextptr);
      for (;;){
	// Wait until status is put back by main to level 1
	wait_1ms(10);
	if (thread_eval_status(contextptr)==1){
	  // the wait function of the main thread should put in debug_info_ptr
	  // the next instruction, here we check for sst/sst_in/cont/kill
	  if (dbgptr->fast_debug_info_ptr){
	    gen test=*dbgptr->fast_debug_info_ptr;
	    if (test.type==_SYMB)
	      test=test._SYMBptr->sommet;
	    if (test.type==_FUNC){
	      if (test==at_sst){
		dbgptr->sst_in_mode=false;
		dbgptr->sst_mode=true;
		return;
	      }
	      if (test==at_sst_in){
		dbgptr->sst_in_mode=true;
		dbgptr->sst_mode=true;
		return;
	      }
	      if (test==at_cont){
		dbgptr->sst_in_mode=false;
		dbgptr->sst_mode=false;
		return;
	      }
	      if (test==at_kill){
		_kill(0,contextptr);
		return;
	      }
	    } // end type _FUNC
	    // eval
	    w[2] = *dbgptr->fast_debug_info_ptr;
	    w[3] = *dbgptr->fast_debug_info_ptr = protecteval(w[2],1,contextptr);
	    *dbgptr->debug_info_ptr=w;
	    dbgptr->debug_refresh=true;
	  } // end if (*dbgptr->debug_info_ptr)
	  thread_eval_status(2,contextptr); // Back to level 2
	} // end if (thread_eval_status()==1)
      } // end endless for loop
    } // end thread debugging
#if (defined WIN32) || (!defined HAVE_SIGNAL_H_OLD)
    *logptr(contextptr) << gettext("Sorry! Debugging requires a true operating system") << endl;
    *logptr(contextptr) << gettext("Please try xcas on Linux or an Unix") << endl;
    return;
#else // WIN32
    if (child_id)
      return;
    vecteur w; 
    // w[0]=[function + args, breakpoints]
    // w[2]= res of last evaluation, 
    // w[3] = next instruction, w[4]=debug_ptr(contextptr)->current_instruction
    // w[5] = watch vector, w[6] = watch values
    // evaluate watch with debug_ptr(contextptr)->debug_allowed=false
    debug_ptr(contextptr)->debug_allowed=false;
    debug_ptr(contextptr)->debug_allowed=true;
    if (!debug_ptr(contextptr)->args_stack.empty()){
      w.push_back(makevecteur(debug_ptr(contextptr)->args_stack.back(),vector_int_2_vecteur(debug_ptr(contextptr)->sst_at,contextptr)));
    }
    else
      w.push_back(undef);
    w.push_back(undef);
    w.push_back(res);
    w.push_back((*debug_ptr(contextptr)->fast_debug_info_ptr));
    (*debug_ptr(contextptr)->fast_debug_info_ptr)=undef;
    w.push_back(debug_ptr(contextptr)->current_instruction);
    w.push_back(debug_ptr(contextptr)->debug_watch);
    w.push_back(undef);
    bool in_debug_loop=true;
    for (;in_debug_loop;){
#ifndef NO_STDEXCEPT
      try {
#endif
	vecteur tmp=gen2vecteur(debug_ptr(contextptr)->debug_watch);
	iterateur it=tmp.begin(),itend=tmp.end();
	for (;it!=itend;++it)
	  *it=it->eval(1,contextptr);
	w[6]=tmp;
#ifndef NO_STDEXCEPT
      }
      catch (std::runtime_error & error){
	last_evaled_argptr(contextptr)=NULL;
	w[6]=string2gen(error.what(),false);
      }
#endif
      ofstream child_out(cas_sortie_name().c_str());
      gen e(symbolic(at_debug,w));
      *logptr(contextptr) << gettext("Archiving ") << e << endl;
      archive(child_out,e,contextptr);
      archive(child_out,zero,contextptr);
      child_out << "Debugging\n" << '¤' ;
      child_out.close();
      kill_and_wait_sigusr2();
      ifstream child_in(cas_entree_name().c_str());
      w[1]= unarchive(child_in,contextptr);
      child_in.close();
      *logptr(contextptr) << gettext("Click reads ") << w[1] << endl;
      if (w[1].type==_SYMB){
	if (w[1]._SYMBptr->sommet==at_sst){
	  debug_ptr(contextptr)->sst_in_mode=false;
	  debug_ptr(contextptr)->sst_mode=true;
	  return;
	}
	if (w[1]._SYMBptr->sommet==at_sst_in){
	  debug_ptr(contextptr)->sst_in_mode=true;
	  debug_ptr(contextptr)->sst_mode=true;
	  return;
	}
	if (w[1]._SYMBptr->sommet==at_cont){
	  debug_ptr(contextptr)->sst_in_mode=false;
	  debug_ptr(contextptr)->sst_mode=false;
	  return;
	}
	if (w[1]._SYMBptr->sommet==at_kill){
	  _kill(0,contextptr);
	}
      }
#ifndef NO_STDEXCEPT
      try {
#endif
	w[2] =w[1].eval(1,contextptr);
#ifndef NO_STDEXCEPT
      }
      catch (std::runtime_error & error ){
	last_evaled_argptr(contextptr)=NULL;
	w[2]=string2gen(error.what(),false);
      }
#endif
    }
#endif // WIN32
  }
#endif // GIAC_HAS_STO_38
#endif // EMCC


  static string printasdouble_deux_points(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    gen a,b;
    check_binary(feuille,a,b);
    string s=b.print(contextptr);
    if (b.type==_SYMB && b._SYMBptr->sommet!=at_of && b._SYMBptr->sommet.ptr()->printsommet)
      s = '('+s+')';
    if (b.type==_FUNC && s.size()>2 && s[0]=='\'' && s[s.size()-1]=='\'')
      s=s.substr(1,s.size()-2);
#ifdef GIAC_HAS_STO_38
    return a.print(contextptr)+"::"+s; // removed final space, otherwise A::B::C adds a _ in prime + " ";
#else
    return a.print(contextptr)+"::"+s+" ";
#endif
  }
  gen symb_double_deux_points(const gen & args){
    return symbolic(at_double_deux_points,args);
  }
  gen _double_deux_points(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    gen a,b,c;
    if (!check_binary(args,a,b))
      return a;
    if (b==at_index)
      return _struct_dot(eval(args,1,contextptr),contextptr);
    if (b==at_revlist || b==at_reverse || b==at_sort || b==at_sorted || b==at_append || b==at_prepend || b==at_concat || b==at_extend || b==at_rotate || b==at_shift || b==at_suppress || b==at_clear ){	
#if 1
	return _struct_dot(args,contextptr);
#else
	return symbolic(at_struct_dot,args);
#endif
    }
    if (b.type==_SYMB){
      unary_function_ptr c=b._SYMBptr->sommet;
      if (c==at_index)
	return _struct_dot(eval(args,1,contextptr),contextptr);
      if (c==at_revlist || c==at_reverse || c==at_sort || c==at_sorted || c==at_append || c==at_prepend || c==at_concat || c==at_extend || c==at_rotate || c==at_shift || c==at_suppress || c==at_clear ){
#if 1
	return _struct_dot(args,contextptr);
#else
	gen d=eval(a,eval_level(contextptr),contextptr);
	if (b._SYMBptr->feuille.type==_VECT && b._SYMBptr->feuille.subtype==_SEQ__VECT && b._SYMBptr->feuille._VECTptr->empty())
	  ;
	else
	  d=makesuite(d,b._SYMBptr->feuille);
	d=c(d,contextptr);
	return sto(d,a,contextptr);
#endif
      }
    }
#if 0
    if (storcl_38 && abs_calc_mode(contextptr)==38 && a.type==_IDNT){
      gen value;
      if (storcl_38(value,a._IDNTptr->id_name,b.type==_IDNT?b._IDNTptr->id_name:b.print().c_str(),undef,false,contextptr,NULL,false)){
	return value;
      }
    }
#endif
#ifndef RTOS_THREADX
#if !defined BESTA_OS && !defined NSPIRE && !defined FXCG
#ifdef HAVE_LIBPTHREAD
    pthread_mutex_lock(&context_list_mutex);
#endif
    if (a.type==_INT_ && a.subtype==0 && a.val>=0 && a.val<(int)context_list().size()){
      context * ptr = context_list()[a.val];
#ifdef HAVE_LIBPTHREAD
      pthread_mutex_unlock(&context_list_mutex);
#endif
      return eval(b,1,ptr);
    }
    if (context_names){
      map<string,context *>::iterator nt=context_names->find(a.print(contextptr)),ntend=context_names->end();
      if (nt!=ntend){
	context * ptr = nt->second;
#ifdef HAVE_LIBPTHREAD
	pthread_mutex_unlock(&context_list_mutex);
#endif
	return eval(b,1,ptr);
      }
    }
#ifdef HAVE_LIBPTHREAD
      pthread_mutex_unlock(&context_list_mutex);
#endif
#endif // RTOS
#endif
    c=b;
    if (b.is_symb_of_sommet(at_of))
      c=b._SYMBptr->feuille[0];
    string cs=c.print(contextptr);
    /* // following code not used since qualified names after export 
       // make b a symbolic not just the function name
    int l=cs.size(),j=0;
    for (;j<l-1;++j){
      if (cs[j]==':' && cs[j+1]==':')
	break;
    }
    if (j==l-1)
    */      
    cs=a.print(contextptr)+"::"+cs;
    std::pair<charptr_gen *,charptr_gen *> p= equal_range(builtin_lexer_functions_begin(),builtin_lexer_functions_end(),charptr_gen(cs.c_str(),0),tri);
    if (p.first!=p.second && p.first!=builtin_lexer_functions_end()){
      c=p.first->second;
      if (b.is_symb_of_sommet(at_of))
	return c(b._SYMBptr->feuille[1],contextptr);
      else
	return c;
    }
    if (b.type==_FUNC) // ? should be != _IDNT 
      return b;
    if (b.type==_SYMB)
      return b.eval(eval_level(contextptr),contextptr);
    return symb_double_deux_points(args);
  }
  static const char _double_deux_points_s []="double_deux_points";
  static define_unary_function_eval2_index(91,__double_deux_points,&_double_deux_points,_double_deux_points_s,&printasdouble_deux_points);
  define_unary_function_ptr5( at_double_deux_points ,alias_at_double_deux_points,&__double_deux_points,_QUOTE_ARGUMENTS,0);

  bool is_binary(const gen & args){
    return (args.type==_VECT) && (args._VECTptr->size()==2) ;
  }

  bool check_binary(const gen & args,gen & a,gen & b){
    if ( (args.type!=_VECT) || (args._VECTptr->size()!=2) ){
      a=gensizeerr(gettext("check_binary"));
      b=a;
      return false;
    }
    a=args._VECTptr->front();
    b=args._VECTptr->back();
    return true;
  }


  // py==0 Xcas, py==1 Python-like, py==2 JS-like
  gen python_xcas(const gen & args,int py,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    // parse in current mode and print in python mode
    gen g(eval(args,1,contextptr));
    int p=python_compat(contextptr);
    if (g.type==_STRNG){
      if (py==0) 
	return string2gen(python2xcas(*g._STRNGptr,contextptr),false);
      g=gen(*g._STRNGptr,contextptr);
    }
    python_compat(py==2?0:1,contextptr);
    string s(g.print(contextptr));
    python_compat(p,contextptr);
    if (py==0)
      return string2gen(python2xcas(s,contextptr),false);
    if (py==2){
      // TODO add function, replace := by =, local by var, -> by nothing
      s=args.print()+": function"+s;
    }
    g=string2gen(s,false);
    return g;
  }

  gen _python(const gen & args,GIAC_CONTEXT){
    return python_xcas(args,1,contextptr);
  }
  static const char _python_s []="python";
  static define_unary_function_eval_quoted (__python,&_python,_python_s);
  define_unary_function_ptr5( at_python ,alias_at_python,&__python,_QUOTE_ARGUMENTS,true);

  gen _xcas(const gen & args,GIAC_CONTEXT){
    return python_xcas(args,0,contextptr);
  }
  static const char _xcas_s []="xcas";
  static define_unary_function_eval_quoted (__xcas,&_xcas,_xcas_s);
  define_unary_function_ptr5( at_xcas ,alias_at_xcas,&__xcas,_QUOTE_ARGUMENTS,true);

  static string printasvirgule(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if ( (feuille.type!=_VECT) || (feuille._VECTptr->size()!=2) )
      return string(sommetstr)+('('+feuille.print(contextptr)+')');
    return feuille._VECTptr->front().print(contextptr)+','+feuille._VECTptr->back().print(contextptr);
  }
  gen _virgule(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return args;
    const_iterateur it=args._VECTptr->begin(),itend=args._VECTptr->end();
    if (itend-it<2)
      return args;
    gen res=makesuite(*it,*(it+1));
    ++it;
    ++it;
    for (;it!=itend;++it)
      res=makesuite(res,*it);
    return res;
  }
  static const char _virgule_s []="virgule";
  static define_unary_function_eval2 (__virgule,&_virgule,_virgule_s,&printasvirgule);
  define_unary_function_ptr( at_virgule ,alias_at_virgule ,&__virgule);

  gen _scientific_format(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return scientific_format(contextptr);
    scientific_format(args.val,contextptr);
    return args;
  }
  static const char _scientific_format_s []="scientific_format";
  static define_unary_function_eval2 (__scientific_format,&_scientific_format,_scientific_format_s,&printasDigits);
  define_unary_function_ptr( at_scientific_format ,alias_at_scientific_format ,&__scientific_format);

  gen _integer_format(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return integer_format(contextptr);
    integer_format(args.val,contextptr);
    return args;
  }
  static const char _integer_format_s []="integer_format";
  static define_unary_function_eval2 (__integer_format,&_integer_format,_integer_format_s,&printasDigits);
  define_unary_function_ptr5( at_integer_format ,alias_at_integer_format,&__integer_format,0,true);

  // 0: xcas, 1: maple, 2: mupad, 3: ti, 256:xcas/python
  gen _xcas_mode(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return xcas_mode(contextptr);
    xcas_mode(contextptr)=args.val & 0xff;
    python_compat(args.val>=256,contextptr);
    return string2gen(gettext("Warning: some commands like subs might change arguments order"),false);
  }
  static const char _xcas_mode_s []="xcas_mode";
  static define_unary_function_eval (__xcas_mode,&_xcas_mode,_xcas_mode_s);
  define_unary_function_ptr5( at_xcas_mode ,alias_at_xcas_mode,&__xcas_mode,0,true);
  static const char _maple_mode_s []="maple_mode";
  static define_unary_function_eval (__maple_mode,&_xcas_mode,_maple_mode_s);
  define_unary_function_ptr5( at_maple_mode ,alias_at_maple_mode,&__maple_mode,0,true);
  gen _python_compat(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return python_compat(contextptr);
    int p=python_compat(contextptr);
    python_compat(args.val,contextptr) ;
    return p;
  }
  static const char _python_compat_s []="python_compat";
  static define_unary_function_eval (__python_compat,&_python_compat,_python_compat_s);
  define_unary_function_ptr5( at_python_compat ,alias_at_python_compat,&__python_compat,0,true);

  gen giac_eval_level(const gen & g,GIAC_CONTEXT){
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return eval_level(contextptr);
    eval_level(contextptr)=args.val;
    DEFAULT_EVAL_LEVEL=args.val;
    return args;
  }
  static const char _eval_level_s []="eval_level";
  static define_unary_function_eval2 (__eval_level,&giac_eval_level,_eval_level_s,&printasDigits);
  define_unary_function_ptr5( at_eval_level ,alias_at_eval_level,&__eval_level,0,true);

  gen _prog_eval_level(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return prog_eval_level(contextptr);
    prog_eval_level_val(contextptr)=args.val;
    return args;
  }
  static const char _prog_eval_level_s []="prog_eval_level";
  static define_unary_function_eval2 (__prog_eval_level,&_prog_eval_level,_prog_eval_level_s,&printasDigits);
  define_unary_function_ptr5( at_prog_eval_level ,alias_at_prog_eval_level,&__prog_eval_level,0,true);

  gen _with_sqrt(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return withsqrt(contextptr);
    withsqrt(contextptr)=(args.val)!=0;
    return args;
  }
  static const char _with_sqrt_s []="with_sqrt";
  static define_unary_function_eval2 (__with_sqrt,&_with_sqrt,_with_sqrt_s,&printasDigits);
  define_unary_function_ptr5( at_with_sqrt ,alias_at_with_sqrt,&__with_sqrt,0,true);

  gen _all_trig_solutions(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return all_trig_sol(contextptr);
    all_trig_sol((args.val)!=0,contextptr);
    return args;
  }
  static const char _all_trig_solutions_s []="all_trig_solutions";
  static define_unary_function_eval2 (__all_trig_solutions,&_all_trig_solutions,_all_trig_solutions_s,&printasDigits);
  define_unary_function_ptr( at_all_trig_solutions ,alias_at_all_trig_solutions ,&__all_trig_solutions);

  gen _complex_mode(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return complex_mode(contextptr);
    complex_mode((args.val)!=0,contextptr);
    return args;
  }
  static const char _complex_mode_s []="complex_mode";
  static define_unary_function_eval2 (__complex_mode,&_complex_mode,_complex_mode_s,&printasDigits);
  define_unary_function_ptr( at_complex_mode ,alias_at_complex_mode ,&__complex_mode);

  gen _keep_algext(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return keep_algext(contextptr);
    keep_algext((args.val)!=0,contextptr);
    return args;
  }
  static const char _keep_algext_s []="keep_algext";
  static define_unary_function_eval2 (__keep_algext,&_keep_algext,_keep_algext_s,&printasDigits);
  define_unary_function_ptr( at_keep_algext ,alias_at_keep_algext ,&__keep_algext);

  gen _angle_radian(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);
    //grad
    //since this is a programming command, not sure what you want done here exactly..., will return 1 for radian, 0 for degree, and 2 for grads to match prior behavior
    if (args.type!=_INT_)
    {
      if(angle_radian(contextptr))
        return 1;
      else if(angle_degree(contextptr))
        return 0;
      else
        return 2;
    }
    //anything but 0 or 2 will result in radians...
    int val = args.val;
    if(val == 0) 
      angle_mode(1, contextptr); //set to degrees if 0
    else if(val==2)
      angle_mode(2, contextptr); //set to grads if 2
    else
      angle_mode(0, contextptr); //set to radians for anything but those two

    return args;
  }
  static const char _angle_radian_s []="angle_radian";
  static define_unary_function_eval2 (__angle_radian,&_angle_radian,_angle_radian_s,&printasDigits);
  define_unary_function_ptr( at_angle_radian ,alias_at_angle_radian ,&__angle_radian);

  gen _epsilon(const gen & arg,GIAC_CONTEXT){
    if ( arg.type==_STRNG &&  arg.subtype==-1) return  arg;
    gen args=evalf_double(arg,0,contextptr);
    if (args.type!=_DOUBLE_)
      return epsilon(contextptr);
    epsilon(fabs(args._DOUBLE_val),contextptr);
    return args;
  }
  static const char _epsilon_s []="epsilon";
  static define_unary_function_eval2 (__epsilon,&_epsilon,_epsilon_s,&printasDigits);
  define_unary_function_ptr( at_epsilon ,alias_at_epsilon ,&__epsilon);

  gen _proba_epsilon(const gen & arg,GIAC_CONTEXT){
    if ( arg.type==_STRNG &&  arg.subtype==-1) return  arg;
    gen args=evalf_double(arg,0,contextptr);
    if (args.type!=_DOUBLE_)
      return proba_epsilon(contextptr);
    proba_epsilon(contextptr)=fabs(args._DOUBLE_val);
    return args;
  }
  static const char _proba_epsilon_s []="proba_epsilon";
  static define_unary_function_eval2 (__proba_epsilon,&_proba_epsilon,_proba_epsilon_s,&printasDigits);
  define_unary_function_ptr( at_proba_epsilon ,alias_at_proba_epsilon ,&__proba_epsilon);

  gen _complex_variables(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return complex_variables(contextptr);
    complex_variables((args.val)!=0,contextptr);
    return args;
  }
  static const char _complex_variables_s []="complex_variables";
  static define_unary_function_eval2 (__complex_variables,&_complex_variables,_complex_variables_s,&printasDigits);
  define_unary_function_ptr( at_complex_variables ,alias_at_complex_variables ,&__complex_variables);

  gen _approx_mode(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);
    if (args.type!=_INT_)
      return approx_mode(contextptr);
    approx_mode((args.val)!=0,contextptr);
    return args;
  }
  static const char _approx_mode_s []="approx_mode";
  static define_unary_function_eval2 (__approx_mode,&_approx_mode,_approx_mode_s,&printasDigits);
  define_unary_function_ptr( at_approx_mode ,alias_at_approx_mode ,&__approx_mode);

  int digits2bits(int n){
#ifdef OLDGNUWINCE
    return (n*33)/10;
#else
    return int(std::floor(std::log(10.0)/std::log(2.0)*n))+1;
#endif
  }

  int bits2digits(int n){
#ifdef OLDGNUWINCE
    return (n*3)/10;
#else
    return int(std::floor(std::log(2.0)/std::log(10.0)*n))+1;
#endif
  }

  void set_decimal_digits(int n,GIAC_CONTEXT){
#ifdef GNUWINCE
    return ;
#else
#ifdef HAVE_LIBMPFR
    decimal_digits(contextptr)=giacmax(absint(n),1);
#else
    decimal_digits(contextptr)=giacmin(giacmax(absint(n),1),13);
#endif
    // deg2rad_g=evalf(cst_pi,1,0)/180;
    // rad2deg_g=inv(deg2rad_g);
#endif
  }

  bool cas_setup(const vecteur & v_orig,GIAC_CONTEXT){
    vecteur v(v_orig);
    for (size_t i=0;i<v.size();++i){
      gen g=v[i];
      if (g.type==_VECT && g._VECTptr->size()==2 && g._VECTptr->front().type==_STRNG)
	v[i]=g._VECTptr->back();
    }
    if (v.size()<7)
      return false;
    if (logptr(contextptr) && debug_infolevel) 
      *logptr(contextptr) << gettext("Cas_setup ") << v << char(10) << char(13) ;
    if (v[0].type==_INT_)
      approx_mode((v[0].val)!=0,contextptr);
    else {
      v[0]=evalf_double(v[0],1,contextptr);
      if (v[0].type==_DOUBLE_)
	approx_mode(v[0]._DOUBLE_val!=0,contextptr);
    }
    if (v[1].type==_INT_)
      complex_variables((v[1].val)!=0,contextptr);
    else {
      v[1]=evalf_double(v[1],1,contextptr);
      if (v[1].type==_DOUBLE_)
	complex_variables(v[1]._DOUBLE_val!=0,contextptr);
    }
    if (v[2].type==_INT_)
      complex_mode((v[2].val)!=0,contextptr);
    else {
      v[2]=evalf_double(v[2],1,contextptr);
      if (v[2].type==_DOUBLE_)
	complex_mode(v[2]._DOUBLE_val!=0,contextptr);
    }
    if (v[3].type==_INT_)
    {
      //grad
      //since end user sees val !=0 being radians, I have hijacked so 2 will be grad, 0 is deg, and anything else is radians
      int val = v[3].val;
      if(val == 0)
        angle_mode(1, contextptr); //degrees if ==0
      else if(val == 2)
        angle_mode(2, contextptr); //grad if ==2
      else
        angle_mode(0, contextptr); //anything else is radians
    }
    else {
      v[3]=evalf_double(v[3],1,contextptr);
      if (v[3].type==_DOUBLE_)
      {
        //grad
        //since end user sees val !=0 being radians, I have hijacked so 2 will be grad, 0 is deg, and anything else is radians
        int val = int(v[3]._DOUBLE_val);
        if(val == 0)
          angle_mode(1, contextptr); //degrees if ==0
        else if(val == 2)
          angle_mode(2, contextptr); //grad if ==2
        else
          angle_mode(0, contextptr); //anything else is radians
      }
    }
    v[4]=evalf_double(v[4],1,contextptr);
    if (v[4].type==_DOUBLE_){
      int format=int(v[4]._DOUBLE_val);
      scientific_format(format % 16,contextptr);
      integer_format(format/16,contextptr);
    }
    v[5]=evalf_double(v[5],1,contextptr);
    if (v[5].type==_DOUBLE_)
      epsilon(fabs(v[5]._DOUBLE_val),contextptr);
    if (v[5].type==_VECT && v[5]._VECTptr->size()==2 && v[5]._VECTptr->front().type==_DOUBLE_ && v[5]._VECTptr->back().type==_DOUBLE_){
      epsilon(fabs(v[5]._VECTptr->front()._DOUBLE_val),contextptr);
      proba_epsilon(contextptr)=fabs(v[5]._VECTptr->back()._DOUBLE_val); 
    }
    if (v[6].type==_INT_)
      set_decimal_digits(v[6].val,contextptr);
    else {
      v[6]=evalf_double(v[6],1,contextptr);
      if (v[6].type==_DOUBLE_)
	set_decimal_digits(int(v[6]._DOUBLE_val),contextptr);
    }
    if (v.size()>=8){
      if (v[7].type==_VECT){
	vecteur & vv =*v[7]._VECTptr;
	if (vv.size()>=4){
	  threads=std::max(1,int(evalf_double(vv[0],1,contextptr)._DOUBLE_val));
	  MAX_RECURSION_LEVEL=std::max(int(evalf_double(vv[1],1,contextptr)._DOUBLE_val),1);
	  debug_infolevel=std::max(0,int(evalf_double(vv[2],1,contextptr)._DOUBLE_val));
	  DEFAULT_EVAL_LEVEL=std::max(1,int(evalf_double(vv[3],1,contextptr)._DOUBLE_val));
	}
      }
    }
    if (v.size()>=9){ 
      if (v[8].type==_INT_)
	increasing_power(v[8].val!=0,contextptr);
      else {
	v[8]=evalf_double(v[8],1,contextptr);
	if (v[8].type==_DOUBLE_)
	  increasing_power(v[8]._DOUBLE_val!=0,contextptr);
      }
    }
    if (v.size()>=10){ 
      if (v[9].type==_INT_)
	withsqrt(v[9].val!=0,contextptr);
      else {
	v[9]=evalf_double(v[9],1,contextptr);
	if (v[9].type==_DOUBLE_)
	  withsqrt(v[9]._DOUBLE_val!=0,contextptr);
      }
    }
    if (v.size()>=11){ 
      if (v[10].type==_INT_)
	all_trig_sol(v[10].val!=0,contextptr);
      else {
	v[10]=evalf_double(v[10],1,contextptr);
	if (v[10].type==_DOUBLE_)
	  all_trig_sol(v[10]._DOUBLE_val!=0,contextptr);
      }
    }
    if (v.size()>=12){ 
      if (v[11].type==_INT_)
	integer_mode(v[11].val!=0,contextptr);
      else {
	v[11]=evalf_double(v[11],1,contextptr);
	if (v[11].type==_DOUBLE_)
	  integer_mode(v[11]._DOUBLE_val!=0,contextptr);
      }
    }
    return true;
  }
  vecteur cas_setup(GIAC_CONTEXT){
    vecteur v;
    v.push_back(approx_mode(contextptr));
    v.push_back(complex_variables(contextptr));
    v.push_back(complex_mode(contextptr));
    int an=angle_mode(contextptr);
    v.push_back(an==2?2:1-an); //grad //not sure if this will mess anything up on your side bernard, having int instead of bool
    v.push_back(scientific_format(contextptr)+16*integer_format(contextptr));
    v.push_back(makevecteur(epsilon(contextptr),proba_epsilon(contextptr)));
    v.push_back(decimal_digits(contextptr));
    v.push_back(makevecteur(threads,MAX_RECURSION_LEVEL,debug_infolevel,DEFAULT_EVAL_LEVEL));
    v.push_back(increasing_power(contextptr));
    v.push_back(withsqrt(contextptr));
    v.push_back(all_trig_sol(contextptr));
    v.push_back(integer_mode(contextptr));
    return v;
  }
  gen _cas_setup(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT){
      vecteur v=cas_setup(contextptr);
      v[0]=makevecteur(string2gen("~",false),v[0]);
      v[1]=makevecteur(string2gen("var C",false),v[1]);
      v[2]=makevecteur(string2gen("C",false),v[2]);
      v[3]=makevecteur(string2gen("0:deg/1:rad/2:grad",false),v[3]);
      v[4]=makevecteur(string2gen("format",false),v[4]);
      v[5]=makevecteur(string2gen("[epsilon,proba_epsilon]",false),v[5]);
      v[6]=makevecteur(string2gen("digits",false),v[6]);
      v[7]=makevecteur(string2gen("[thread,recursion,debug,eval]",false),v[7]);
      v[8]=makevecteur(string2gen("increasing power",false),v[8]);      
      v[9]=makevecteur(string2gen("sqrt",false),v[9]);
      v[10]=makevecteur(string2gen("trig. solutions",false),v[10]);
      v[11]=makevecteur(string2gen("integer",false),v[11]);
      return v;
    }
    vecteur & w=*args._VECTptr;
    if (w.empty())
      return cas_setup(contextptr);
    if (!cas_setup(w,contextptr))
      return gendimerr(contextptr);
#ifdef HAVE_SIGNAL_H_OLD
    if (!child_id){
      _signal(symbolic(at_quote,symbolic(at_cas_setup,w)),contextptr);
    }
#endif
    return args;
  }
  static const char _cas_setup_s []="cas_setup";
  static define_unary_function_eval (__cas_setup,&_cas_setup,_cas_setup_s);
  define_unary_function_ptr5( at_cas_setup ,alias_at_cas_setup,&__cas_setup,0,true);

  string printasDigits(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if (feuille.type==_VECT && feuille._VECTptr->empty())
      return sommetstr;
    return sommetstr+(" := "+feuille.print(contextptr));
  }
  gen _Digits(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return decimal_digits(contextptr);
    set_decimal_digits(args.val,contextptr);
    _cas_setup(cas_setup(contextptr),contextptr);
    return decimal_digits(contextptr);
  }
  static const char _Digits_s []="Digits";
  static define_unary_function_eval2 (__Digits,&_Digits,_Digits_s,&printasDigits);
  define_unary_function_ptr( at_Digits ,alias_at_Digits ,&__Digits);


  class gen_sort {
    gen sorting_function;
    const context * contextptr;
  public:
    bool operator () (const gen & a,const gen & b){
      gen ab(makesequence(a,b));
      gen c=sorting_function(ab,contextptr);
      if (c.type!=_INT_){
#ifndef NO_STDEXCEPT
	setsizeerr(gettext("Unable to sort ")+sorting_function.print(contextptr)+"("+ab.print(contextptr)+")="+c.print(contextptr));
#else
	*logptr(contextptr) << gettext("Unable to sort ")+sorting_function.print(contextptr)+"("+ab.print(contextptr)+")="+c.print(contextptr) << endl;
#endif
	return true;
      }
      return !is_zero(c);
    }
    gen_sort(const gen & f,const context * ptr): sorting_function(f),contextptr(ptr) {};
    gen_sort(): sorting_function(at_inferieur_strict),contextptr(0) {};
  };

  /*
  gen sorting_function;
  bool sort_sort(const gen & a,const gen & b){
    gen c=sorting_function(gen(makevecteur(a,b),_SEQ__VECT),0);
    if (c.type!=_INT_)
      setsizeerr(gettext("Unable to sort ")+c.print(contextptr));
    return !is_zero(c);
  }
  */

  /*
  gen negdistrib(const gen & g,GIAC_CONTEXT){
    if (!g.is_symb_of_sommet(at_plus))
      return -g;
    return _plus(-g._SYMBptr->feuille,contextptr);
  }
  */

  gen simplifier(const gen & g,GIAC_CONTEXT){
#if 0 // def NSPIRE
    return g;
#endif
    if (g.type!=_SYMB || g._SYMBptr->sommet==at_program || g._SYMBptr->sommet==at_pnt || g._SYMBptr->sommet==at_unit || g._SYMBptr->sommet==at_integrate || g._SYMBptr->sommet==at_superieur_strict || g._SYMBptr->sommet==at_superieur_egal || g._SYMBptr->sommet==at_inferieur_strict || g._SYMBptr->sommet==at_inferieur_egal || g._SYMBptr->sommet==at_and || g._SYMBptr->sommet==at_ou || g._SYMBptr->sommet==at_not || g._SYMBptr->sommet==at_xor || g._SYMBptr->sommet==at_piecewise
#ifndef FXCG
	|| g._SYMBptr->sommet==at_archive
#endif
	)
      return g;
    if (is_equal(g))
      return apply_to_equal(g,simplifier,contextptr);
    if (is_inf(g))
      return g;
    vecteur v(lvar(g)),w(v);
    for (unsigned i=0;i<w.size();++i){
      gen & wi=w[i];
      if (wi.type==_SYMB){
	gen f =wi._SYMBptr->feuille;
	if (wi.is_symb_of_sommet(at_pow) && f.type==_VECT && f._VECTptr->size()==2 && f._VECTptr->back().type==_FRAC){
	  gen base=f._VECTptr->front();
	  gen d= f._VECTptr->back()._FRACptr->den;
	  if ( (base.type==_INT_ && absint(base.val)>3) || base.type==_ZINT){
	    gen b=evalf_double(base,1,contextptr);
	    if (b.type==_DOUBLE_){
	      double bd=b._DOUBLE_val;
	      bd=std::log(bd);
	      int N=int(bd/std::log(2.0));
	      int ii;
	      for (ii=3;ii<=N;++ii){
		double di=std::exp(bd/ii);
		gen g=exact_double(di,1e-15);
		if (is_integer(g) && pow(g,ii,contextptr)==base){
		  wi=pow(g,ii*f._VECTptr->back(),contextptr);
		  break;
		}
	      }
	      if (ii!=N+1)
		continue;
	    }
	  }
	  if (d.type==_INT_){
	    gen f0=simplifier(f._VECTptr->front(),contextptr);
	    gen z=fast_icontent(f0);
	    gen n= f._VECTptr->back()._FRACptr->num;
	    if (d.val<0){ n=-n; d=-d;}
	    gen zn=pow(z,n,contextptr),a,b,fapprox;
	    bool pos; // pos should be true after next call since zn is > 0
	    zint2simpldoublpos(zn,a,b,pos,d.val,contextptr);
	    if (pos){
	      if (0 && n==1)
		wi=b*pow(fast_divide_by_icontent(f0,z/a),f._VECTptr->back(),contextptr);
	      else { // avoid extracting sqrt(2) out for simplify(exp(i*pi/5));
		if (d*f._VECTptr->back()==1 && has_evalf(f0,fapprox,1,contextptr))
		  wi=b*pow(a*fast_divide_by_icontent(f0,z),inv(d,contextptr),contextptr);
		else
		  wi=b*pow(a,inv(d,contextptr),contextptr)*pow(fast_divide_by_icontent(f0,z),f._VECTptr->back(),contextptr);
	      }
	      continue;
	    }
	  }
	}
	// check added for [] and () otherwise xcas_mode index shift is not taken in account
	if (wi._SYMBptr->sommet==at_at || wi._SYMBptr->sommet==at_of || wi._SYMBptr->sommet==at_ans)
	  wi=symbolic(wi._SYMBptr->sommet,simplifier(f,contextptr));
	else
	  wi=wi._SYMBptr->sommet(simplifier(f,contextptr),contextptr);
      }
    }
    gen g_(g);
    if (v!=w)
      g_=subst(g,v,w,false,contextptr);
    //g_=aplatir_fois_plus(g_);
    // distribute neg over +
    //vector< gen_op_context > negdistrib_v(1,negdistrib);
    //vector<const unary_function_ptr *> neg_v(1,at_neg);
    //g_=subst(g_,neg_v,negdistrib_v,false,contextptr);
    return liste2symbolique(symbolique2liste(g_,contextptr));
  }
  gen _simplifier(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    if (is_equal(g))
      return apply_to_equal(g,_simplifier,contextptr);
    if (g.type!=_VECT)
      return simplifier(g,contextptr);
    return apply(g,_simplifier,contextptr);
  }

  gen _inferieur_strict_sort(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return gensizeerr(contextptr);
    gen a=args._VECTptr->front(),b=args._VECTptr->back();
    if (a.type==_VECT && b.type==_VECT){
      unsigned as=unsigned(a._VECTptr->size()),bs=unsigned(b._VECTptr->size());
      for (unsigned i=0;i<as && i<bs;++i){
	if ((*a._VECTptr)[i]!=(*b._VECTptr)[i]){
	  a=(*a._VECTptr)[i]; b=(*b._VECTptr)[i];
	  break;
	}
      }
    }
    if (a.is_symb_of_sommet(at_equal) && b.is_symb_of_sommet(at_equal)){
      if (a._SYMBptr->feuille[0]!=b._SYMBptr->feuille[0]){
	a=a._SYMBptr->feuille[0]; b=b._SYMBptr->feuille[0];
      }
      else {
	a=a._SYMBptr->feuille[1]; b=b._SYMBptr->feuille[1];
      }
    }
    if (a.type==_STRNG){
      if (b.type!=_STRNG) return true;
      return *a._STRNGptr<*b._STRNGptr;
    }
    if (b.type==_STRNG)
      return false;
    gen res=inferieur_strict(a,b,contextptr);
    if (res.type==_INT_)
      return res;
    return islesscomplexthanf(a,b);
  }
  static const char _inferieur_strict_sort_s []="inferieur_strict_sort";
  static define_unary_function_eval (__inferieur_strict_sort,&_inferieur_strict_sort,_inferieur_strict_sort_s);
  define_unary_function_ptr5( at_inferieur_strict_sort ,alias_at_inferieur_strict_sort,&__inferieur_strict_sort,0,true);

  gen * qs_comp_f=0;
  const context * qs_contextptr=0;
  int qs_comp(const void * a_,const void * b_){
    const gen * a=(const gen *)a_;
    const gen * b=(const gen *)b_;
    if (*a==*b) return 0;
    if (qs_comp_f){
      gen c=(*qs_comp_f)(makesequence(*a,*b),qs_contextptr);
      if (c.type==_INT_)
	return c.val==0?1:-1;
    }
    return 1;
  }

  gen _sort(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_SYMB)
      return simplifier(args,contextptr);
    if (args.type!=_VECT)
      return args; // FIXME sort in additions, symbolic(at_sort,args);
    vecteur v=*args._VECTptr;
    int subtype;
    gen f;
    bool usersort=v.size()==2 && v[0].type==_VECT && v[1].type!=_VECT
      // && args.subtype==_SEQ__VECT
      ;
    bool rev=false;
    if (usersort){
      f=v[1];
      subtype=v[0].subtype;
      v=*v[0]._VECTptr;
      if (is_equal(f) && f._SYMBptr->feuille[0]==at_reverse && !is_zero(f._SYMBptr->feuille[1])){
	f=at_inferieur_strict;
	rev=true;
      }
    }
    else {
      f=at_inferieur_strict_sort;
      subtype=args.subtype;
    }
    if (!v.empty() && (f==at_inferieur_strict || f==at_inferieur_strict_sort)){
      // check integer or double vector
      if (v.front().type==_INT_ && is_integer_vecteur(v,true)){
	// find min/max
	vector<int> w(vecteur_2_vector_int(v));
	int m=giacmin(w),M=giacmax(w);
	if (M-m<=int(w.size())/3){
	  vector<int> eff(M-m+1);
	  effectif(w,eff,m);
	  vecteur res(w.size());
	  iterateur it=res.begin();
	  int val=m;
	  for (unsigned i=0;i<eff.size();++val,++i){
	    unsigned I=eff[i];
	    for (unsigned j=0;j<I;++it,++j){
	      *it=val;
	    }
	  }
	  if (rev)
	    vreverse(res.begin(),res.end());
	  return gen(res,subtype);
	}
	sort(w.begin(),w.end());
	vector_int2vecteur(w,v);
	if (rev)
	  vreverse(v.begin(),v.end());
	return gen(v,subtype);
      }
      vector<giac_double> V;
      if (v.front().type==_DOUBLE_ && is_fully_numeric(v) && convert(v,V,true)){
	sort(V.begin(),V.end());
	v=vector_giac_double_2_vecteur(V);
	if (rev)
	  vreverse(v.begin(),v.end());
	return gen(v,subtype);
      }
    }
    qs_comp_f = &f;
    qs_contextptr=contextptr;
    qsort(v.begin(),v.size(),sizeof(gen),qs_comp);
    // sort(v.begin(),v.end(),gen_sort(f,contextptr));
    if (rev)
      vreverse(v.begin(),v.end());
    return gen(v,subtype);
  }
  static const char _sort_s []="sort";
  static define_unary_function_eval (__sort,&_sort,_sort_s);
  define_unary_function_ptr5( at_sort ,alias_at_sort,&__sort,0,true);

  static const char _sorted_s []="sorted";
  static define_unary_function_eval (__sorted,&_sort,_sorted_s);
  define_unary_function_ptr5( at_sorted ,alias_at_sorted,&__sorted,0,true);

  static gen remove_nodisp(const gen & g){
    if (g.is_symb_of_sommet(at_nodisp))
      return g._SYMBptr->feuille;
    return g;
  }
  gen _ans(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_VECT && args._VECTptr->size()>1){
      vecteur v=*args._VECTptr;
      gen tmp=_ans(v.front(),contextptr);
      v.erase(v.begin());
      return _at(makesequence(tmp,gen(v,args.subtype)),contextptr);
    }
    int s=int(history_out(contextptr).size());
    if (!s)
      return undef;
    int i;
    if (args.type!=_INT_)
      i=-1;
    else {
      i=args.val;
    }
    if (i>=0){
      if (i>=s)
	return gentoofewargs(print_INT_(i));
      return remove_nodisp(history_out(contextptr)[i]);
    }
    if (s+i<0)
      return gentoofewargs(print_INT_(-i));
    return remove_nodisp(history_out(contextptr)[s+i]);
  }
  static const char _ans_s []="ans";
  static define_unary_function_eval (__ans,&_ans,_ans_s);
  define_unary_function_ptr5( at_ans ,alias_at_ans,&__ans,0,true);

  vector<int> float2continued_frac(double d_orig,double eps){
    if (eps<1e-11)
      eps=1e-11;
    double d=fabs(d_orig);
    vector<int> v;
    if (d>rand_max2){
#ifndef NO_STDEXCEPT
      setsizeerr(gettext("Float too large"));
#endif
      v.push_back(rand_max2);
      return v;
    }
    double i;
    for (;;){
      i=std::floor(d);
      v.push_back(int(i));
      d=d-i;
      if (d<eps)
	return v;
      d=1/d;
      eps=eps*d*d;
    }
  }

  gen continued_frac2gen(vector<int> v,double d_orig,double eps,GIAC_CONTEXT){
    gen res(v.back());
    for (;;){
      v.pop_back();
      if (v.empty()){
	if (
	    !my_isnan(d_orig) &&
	    fabs(evalf_double(res-d_orig,1,contextptr)._DOUBLE_val)>eps)
	  return d_orig;
	return res;
      }
      res=inv(res,contextptr);
      res=res+v.back();
    }
    return res;
  }

  gen chk_not_unit(const gen & g){
    return g;
  }



  gen denest_sto(const gen & g){
    if (g.type==_VECT){
      vecteur v(*g._VECTptr);
      iterateur it=v.begin(),itend=v.end();
      for (;it!=itend;++it)
	*it=denest_sto(*it);
      return gen(v,g.subtype);
    }
    if (g.is_symb_of_sommet(at_sto) && g._SYMBptr->feuille.type==_VECT && g._SYMBptr->feuille._VECTptr->size()==2){
      gen b=g._SYMBptr->feuille._VECTptr->front();
      gen a=g._SYMBptr->feuille._VECTptr->back();
      // a:=b, should be a0,..,an-2,an-1=b
      if (b.is_symb_of_sommet(at_sto)){
	// if b is a sto a0,...an-2,a1=c[0],c1,..,cn-1
	gen c=denest_sto(b);
	if (a.type==_VECT && c.type==_VECT){
	  vecteur av=*a._VECTptr;
	  vecteur cv=*c._VECTptr;
	  av.back()=symb_equal(av.back(),cv.front()); // symbolic(at_equal,makesequence(av.back(),cv.front()));
	  cv.erase(cv.begin());
	  return gen(mergevecteur(av,cv),_SEQ__VECT);
	}
      }
      if (a.type==_VECT){
	vecteur av=*a._VECTptr;
	av.back()=symb_equal(av.back(),b); // symbolic(at_equal,makesequence(av.back(),b));
	return gen(av,_SEQ__VECT);
      }
    }
    if (g.type==_SYMB){
      gen f=denest_sto(g._SYMBptr->feuille);
      return symbolic(g._SYMBptr->sommet,f);
    }
    return g;
  }
  
  gen _convert(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT){
      if (args.type==_POLY)
	return _convert(vecteur(1,args),contextptr);
      return gensizeerr(contextptr);
    }
    vecteur v=*args._VECTptr;
    int s=int(v.size());
    if (s==3 && v[0].type==_INT_ && v[0].subtype==_INT_PLOT && v[0].val==_AXES && v[2].type==_INT_ && v[2].subtype==_INT_PLOT && v[2].val==_SET__VECT){
      // axes.set(xlabel="",ylabel="")
      return v[1].type==_VECT?change_subtype(v[1],_SEQ__VECT):v[1];
    }
    if (s>=1 && v.front().type==_POLY){
      int dim=v.front()._POLYptr->dim;
      vecteur idx(dim);
      vector< monomial<gen> >::const_iterator it=v.front()._POLYptr->coord.begin(),itend=v.front()._POLYptr->coord.end();
      vecteur res;
      res.reserve(itend-it);
      for (;it!=itend;++it){
	index_t::const_iterator j=it->index.begin();
	for (int k=0;k<dim;k++,++j)
	  idx[k]=*j;
	res.push_back(makevecteur(it->value,idx));
      }
      return res;
    }
    if (s<2)
      return gensizeerr(contextptr);
    gen f=v[1];
    gen g=v.front();
    if (g.type==_VECT && v[1].type==_VECT)
      return gen(*g._VECTptr,v[1].subtype);
    if (s>=2 && f==at_series){
      if (g.type==_VECT){
	sparse_poly1 res=vecteur2sparse_poly1(*g._VECTptr);
	if (s>=3 && v[2].type==_INT_ && v[2].val>=0)
	  res.push_back(monome(undef,v[2].val));
	return res;
      }
      int s=series_flags(contextptr);
      series_flags(contextptr) = s | (1<<4);
      v.erase(v.begin()+1);
      gen res=_series(gen(v,args.subtype),contextptr);
      series_flags(contextptr) = s ;
      return res;
    }
    if (s==3 && f.type==_INT_ ){
      if (f.val==_BASE && is_integer(v.back()) ){
	if (is_greater(1,v.back(),contextptr))
	  return gensizeerr(gettext("Bad conversion basis"));
	if (is_integer(g)){
	  if (is_zero(g))
	    return makevecteur(g);
	  // convert(integer,base,integer)
	  bool positif=is_positive(g,contextptr);
	  g=abs(g,contextptr);
	  vecteur res;
	  gen q;
	  for (;!is_zero(g);){
	    res.push_back(irem(g,v.back(),q));
	    g=q;
	  }
	  // vreverse(res.begin(),res.end());
	  if (positif)
	    return res;
	  return -res;
	}
	if (g.type==_VECT){
	  vecteur w(*g._VECTptr);
	  vreverse(w.begin(),w.end());
	  return horner(w,v.back());
	}
      }
      if (f.val==_CONFRAC && v.back().type==_IDNT){
	g=evalf_double(g,1,contextptr);
	if (g.type==_DOUBLE_)
	  return sto(vector_int_2_vecteur(float2continued_frac(g._DOUBLE_val,epsilon(contextptr))),v.back(),contextptr);
      }
    }
    if (s>2)
      g=gen(mergevecteur(vecteur(1,g),vecteur(v.begin()+2,v.begin()+s)),args.subtype);
    if (g.type==_STRNG){
      string s=*g._STRNGptr;
      int i=f.type==_INT_?f.val:-1;
      if (i==_IDNT)
	return identificateur(s);
      if (i==_INT_ || i==_ZINT || f==at_int){
	f=gen(s,contextptr);
	if (!is_integral(f)) 
	  return gensizeerr(contextptr);
	return f;
      }
      if (i==_SYMB || i==_FRAC)
	return gen(s,contextptr);
      if (i==_FRAC || f==at_frac){
	f=exact(gen(s,contextptr),contextptr);
	return f;
      }
      if (i==_REAL)
	return evalf(gen(s,contextptr),1,contextptr);
      if (i==_DOUBLE_ || f==at_real || f==at_float)
	return evalf_double(gen(s,contextptr),1,contextptr);
    }
#ifndef CAS38_DISABLED
    if (v[1].type==_FUNC){
      if (f==at_sincos)
	return sincos(g,contextptr);
      if (f==at_sin)
	return trigsin(g,contextptr);
      if (f==at_cos)
	return trigcos(g,contextptr);
      if (f==at_tan)
	return halftan(g,contextptr);
      if (f==at_plus)
	return partfrac(tcollect(g,contextptr),true,contextptr);
      if (f==at_prod){
	if (is_integer(g)) return _ifactor(g,contextptr);
	return _factor(_texpand(g,contextptr),contextptr);
      }
      if (f==at_division)
	return _simplify(g,contextptr);
      if (f==at_exp || f==at_ln )
	return trig2exp(g,contextptr);
      if (f==at_string){
	int maxp=MAX_PRINTABLE_ZINT;
	MAX_PRINTABLE_ZINT= 1000000;
	gen res=string2gen(g.print(contextptr),false);
	MAX_PRINTABLE_ZINT=maxp;
	return res;
      }
      if (g.type==_MAP){
	if (f==at_matrix || f==at_vector){
	  if (g.subtype==_SPARSE_MATRIX)
	    *logptr(contextptr) << gettext("Run convert(matrix,array) for dense conversion") << endl;
	  g.subtype=_SPARSE_MATRIX;
	  return g;
	}
	if (f==at_table){
	  g.subtype=0;
	  return g;
	}
      }
      if (g.type==_VECT){
	if (f==at_matrix){
	  if (!ckmatrix(g))
	    return gentypeerr(contextptr);
	  g.subtype=_MATRIX__VECT;
	  return g;
	}
	if ( f==at_vector ){
	  g.subtype=_MATRIX__VECT;
	  return g;
	}
      }
      return f(g,contextptr);
      // setsizeerr();
    }
#endif
    if (f.type==_INT_ && f.val>=0) {
      if (f.val==_CONFRAC){
	if (g.type==_VECT)
	  return _dfc2f(g,contextptr);
	g=evalf_double(g,1,contextptr);
	if (g.type==_DOUBLE_)
	  return vector_int_2_vecteur(float2continued_frac(g._DOUBLE_val,epsilon(contextptr)));
      }
      int i=f.val;
      if (f.val==_FRAC && f.subtype==_INT_TYPE)
	return exact(g,contextptr);
      if (f.val==_POLY1__VECT && f.subtype==_INT_MAPLECONVERSION){ // remove order_size
	if (g.type==_SPOL1){
	  vecteur v; int shift;
	  if (sparse_poly12vecteur(*g._SPOL1ptr,v,shift)){
	    if (is_undef(v.front()))
	      v.erase(v.begin());
	    v=trim(v,0);
	    if (shift)
	      return makesequence(gen(v,_POLY1__VECT),shift);
	    return gen(v,_POLY1__VECT);
	  }
	  return gensizeerr(contextptr);
	}
	if (g.type==_VECT && !g._VECTptr->empty()){
	  // check if g is a list of [coeff,[index]]
	  vecteur & w=*g._VECTptr;
	  if (w.front().type!=_VECT)
	    return change_subtype(g,_POLY1__VECT);
	  if (w.front().type==_VECT && w.front()._VECTptr->size()==2 && w.front()._VECTptr->back().type==_VECT){
	    unsigned dim=unsigned(w.front()._VECTptr->back()._VECTptr->size());
	    iterateur it=w.begin(),itend=w.end();
	    polynome res(dim);
	    vector< monomial<gen> > & coord =res.coord;
	    coord.reserve(itend-it);
	    index_t i(dim);
	    for (;it!=itend;++it){
	      if (it->type!=_VECT || it->_VECTptr->size()!=2 || it->_VECTptr->back().type!=_VECT)
		break;
	      vecteur & idx = *it->_VECTptr->back()._VECTptr;
	      if (idx.size()!=dim)
		break;
	      const_iterateur jt=idx.begin(),jtend=idx.end();
	      for (int k=0;jt!=jtend;++jt,++k){
		if (jt->type!=_INT_)
		  break;
		i[k]=jt->val;
	      }
	      if (jt!=jtend)
		break;
	      coord.push_back(monomial<gen>(it->_VECTptr->front(),i));
	    }
	    if (it==itend)
	      return res;
	  }
	}
	vecteur l(lop(g,at_order_size));
	vecteur lp(l.size(),zero);
	g=subst(g,l,lp,false,contextptr);
	return g;
      }
#ifndef CAS38_DISABLED
      if (f.subtype==_INT_MAPLECONVERSION){
	switch (i){
	case _TRIG:
	  return sincos(g,contextptr);
	case _EXPLN:
	  return trig2exp(g,contextptr);
	case _PARFRAC: case _FULLPARFRAC:
	  return _partfrac(g,contextptr);
	case _MAPLE_LIST:
	  if (g.subtype==0 && ckmatrix(g)){
	    vecteur v;
	    aplatir(*g._VECTptr,v);
	    return v;
	  }
	  g.subtype=0;
	  return g;
	default:
	  return gensizeerr(contextptr);
	}
      }
#endif
      g.subtype=v.back().val;
      return g;
    }
    return gensizeerr(contextptr);
  }
  static const char _convert_s []="convert";
  static define_unary_function_eval (__convert,&_convert,_convert_s);
  define_unary_function_ptr5( at_convert ,alias_at_convert,&__convert,0,true);

  gen _deuxpoints(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return symbolic(at_deuxpoints,args);
  }
  static const char _deuxpoints_s []=":";
  static define_unary_function_eval4 (__deuxpoints,&_deuxpoints,_deuxpoints_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_deuxpoints ,alias_at_deuxpoints ,&__deuxpoints);

  bool is_address(const gen & g,size_t & addr){
    if (g.type==_INT_){
      addr=g.val;
      return true;
    }
    if (g.type!=_ZINT)
      return false;
    addr = modulo(*g._ZINTptr,(unsigned)0x80000000);
    addr += 0x80000000;
    return true;
  }
  gen _read(const gen & args_,GIAC_CONTEXT){
    gen args(args_);
    size_t addr;
    if (is_address(args,addr))
      return (int) *(unsigned char *) addr;
    if (args.type!=_STRNG || args.subtype==-1)
      return gensizeerr(contextptr);
    char filename[512];
    sprintf(filename, "\\\\fls0\\%s", args._STRNGptr->c_str());
    FILE * f=fopen(filename,"r");
    if (!f){
      sprintf(filename, "\\\\fls0\\%s.py", args._STRNGptr->c_str());
      f=fopen(filename,"r");
      if (!f){
	sprintf(filename, "\\\\fls0\\%s.txt", args._STRNGptr->c_str());
	f=fopen(filename,"r");
	if (!f)
	  return 0;
      }
    }
    *logptr(contextptr) << "Opening " << filename << endl;
    string s;
    for (;!feof(f);){
      char c=fgetc(f);
      if (c==EOF || c==0)
	break;
      s += c;
    }
    fclose(f);
    //*logptr(contextptr) << "Reading " << s << endl;
    gen g(s,contextptr);
    g=eval(g,1,contextptr);
    return g;
  }   
  static const char _read_s []="read";
  static define_unary_function_eval (__read,&_read,_read_s);
  define_unary_function_ptr5( at_read ,alias_at_read ,&__read,0,T_RETURN);

  gen _read32(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    size_t addr;
    if (args.type==_VECT && args._VECTptr->size()==2 && args._VECTptr->back().type==_INT_){
      int n=args._VECTptr->back().val;
      if (n<=0 || !is_address(args._VECTptr->front(),addr)) 
	return undef;
      vecteur res;
      for (int i=0;i<n;++i){
	res.push_back(makevecteur((longlong) addr,(longlong) *(unsigned *) addr));
	addr += 4;
      }
      return res;
    }
    if (is_address(args,addr))
      return (longlong) *(unsigned *) addr;
    return gensizeerr(contextptr);
  }
  static const char _read32_s []="read32";
  static define_unary_function_eval (__read32,&_read32,_read32_s);
  define_unary_function_ptr5( at_read32 ,alias_at_read32 ,&__read32,0,true);

  gen _write32(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return _read32(args,contextptr);
    if (args.type==_VECT){
      vecteur v=*args._VECTptr;
      size_t addr;
      if (v.size()==2 && is_address(v.front(),addr)){
	gen vb=v.back();
	unsigned * ptr =(unsigned *) addr;
	if (vb.type==_INT_){
	  *ptr=vb.val;
	  return makevecteur(longlong(addr),longlong(*ptr));
	}
	if (vb.type==_ZINT){
	  unsigned l =mpz_get_si(*vb._ZINTptr);
	  *ptr=l;
	  return makevecteur(longlong(addr),longlong(*ptr));
	}
      }
    }
    return gensizeerr(contextptr);
  }
  static const char _write32_s []="write32";
  static define_unary_function_eval (__write32,&_write32,_write32_s);
  define_unary_function_ptr5( at_write32 ,alias_at_write32,&__write32,0,true);

  void add_py(string & fname,GIAC_CONTEXT){
    const char * ptr=fname.c_str();
    int s=strlen(ptr);
    for (int i=0;i<s;++i){
      if (ptr[i]=='.'){
	return;
      }
    }
    fname += ".py";
    python_compat(1,contextptr);
  }
  gen _write(const gen & args,GIAC_CONTEXT){
#if 1
    if (args.type==_VECT){
      vecteur v=*args._VECTptr;
      size_t addr;
      if (v.size()==2 && is_address(v.front(),addr) && v.back().type==_INT_){
	unsigned char * ptr =(unsigned char *) addr;
	// int res=*ptr;
	*ptr=v.back().val;
	return *ptr;//return res;
      }
    }
    return undef;
#else
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    string res,fname;
    if (args.type==_VECT){
      vecteur v=*args._VECTptr;
      v.front()=eval(v.front(),eval_level(contextptr),contextptr);
      if (v.size()<2 || v.front().type!=_STRNG)
	return gensizeerr(contextptr);
      if (v.size()==2 && is_zero(v[1])){
	v[1]=_VARS(0,contextptr);
	if (v[1].type==_VECT)
	  v=mergevecteur(vecteur(1,v[0]),*v[1]._VECTptr);
      }
      bool b=python_compat(contextptr);
      fname=*v[0]._STRNGptr;
      add_py(fname,contextptr);
      const_iterateur it=v.begin()+1,itend=v.end();
      for (;it!=itend;++it){
	if (it->type==_IDNT){
	  gen tmp=eval(*it,1,contextptr);
	  gen tmp2=*it;
	  res += symb_sto(tmp,tmp2).print(contextptr);
	  res += ';';
	  res += '\n';
	  res += char(0);
	}
      }
      python_compat(b,contextptr);      
    }
    else {
      if (args.type!=_STRNG)
	return symbolic(at_write,args);
      fname=*args._STRNGptr;
      const_iterateur it=history_in(contextptr).begin(),itend=history_in(contextptr).end();
      if (it==itend)
	return zero;
      bool b=python_compat(contextptr);
      add_py(fname,contextptr);
      for (;it!=itend;++it){
	if (!it->is_symb_of_sommet(at_write)){
	  res += it->print(contextptr);
	  res += ';';
	  res +=  '\n';
	  res += char(0);
	}
      }
      python_compat(b,contextptr);      
    }
    if (fname.size()>200) fname=fname.substr(0,200);
    fname = "\\\\fls0\\"+fname;
    *logptr(contextptr) << "Write "<<fname << endl;
    size_t size=fname.size();    
    unsigned short pFile[256];
    Bfile_StrToName_ncpy(pFile, fname.c_str(), size+1);
    int hFile = Bfile_OpenFile_OS(pFile, READWRITE); // Get handle
    if(hFile < 0) {
      // error. file does not exist yet. try creating it
      int size = 1;
      Bfile_CreateEntry_OS(pFile, CREATEMODE_FILE, &size);
      // now try opening
      hFile = Bfile_OpenFile_OS(pFile, READWRITE); // Get handle
      if(hFile < 0) return 0; // if it still fails, there's nothing we can do
    }
    Bfile_WriteFile_OS(hFile, res, res.size());
    Bfile_CloseFile_OS(hFile);
    return plus_one;
#endif
  }
  static const char _write_s []="write";
  static define_unary_function_eval_quoted (__write,&_write,_write_s);
  define_unary_function_ptr5( at_write ,alias_at_write,&__write,_QUOTE_ARGUMENTS,true);

  gen symb_findhelp(const gen & args){
    return symbolic(at_findhelp,args);
  }
  gen _findhelp(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    int lang=language(contextptr);
    int helpitems = 0;
    if (g.type==_VECT && g.subtype==_SEQ__VECT && g._VECTptr->size()==2 && g._VECTptr->back().type==_INT_){
      args=g._VECTptr->front();
      lang=absint(g._VECTptr->back().val);
    }
    if (args.type==_IDNT)
      args=eval(args,1,contextptr);
    if (args.type==_SYMB && args._SYMBptr->sommet==at_of && args._SYMBptr->feuille.type==_VECT && args._SYMBptr->feuille._VECTptr->size()==2){
      args=args._SYMBptr->feuille._VECTptr->front();
      args=eval(args,1,contextptr);
    }
    if (args.type==_SYMB && args._SYMBptr->sommet!=at_of){
      if (args._SYMBptr->sommet==at_program && args._SYMBptr->feuille.type==_VECT){
	vecteur v=*args._SYMBptr->feuille._VECTptr;
	if (v.size()==3){
	  string argss="Help on user function "+g.print(contextptr)+"(";
	  if (v[0].type==_VECT && v[0].subtype==_SEQ__VECT && v[0]._VECTptr->size()==1)
	    argss += v[0]._VECTptr->front().print(contextptr);
	  else
	    argss += v[0].print(contextptr);
	  gen g=v[2];
	  while (g.is_symb_of_sommet(at_bloc)) 
	    g=g._SYMBptr->feuille;
	  while (g.is_symb_of_sommet(at_local))
	    g=g._SYMBptr->feuille[1];
	  while (g.type==_VECT && !g._VECTptr->empty())
	    g=g._VECTptr->front();
	  argss += ")\n";
	  if (g.type==_STRNG)
	    argss += *g._STRNGptr;
	  else {
	    argss += "Begins by: ";
	    argss +=g.print(contextptr);
	  }
	  return string2gen(argss,false);
	}
      }
      args=args._SYMBptr->sommet;
    }
    string argss=args.print(contextptr);
    // remove space at the end if required
    while (!argss.empty() && argss[argss.size()-1]==' ')
      argss=argss.substr(0,argss.size()-1);
    const char * howto, * syntax, * related, *examples;
    if (has_static_help(argss.c_str(),lang,howto,syntax,examples,related)){
#ifdef NSPIRE
      if (argss.size()>2 && argss[0]=='\'' && argss[argss.size()-1]=='\'')
	argss=argss.substr(1,argss.size()-2);
      COUT << howto << endl << "Syntax: " << argss << "(" << syntax << ")" << endl << "See also: " << related << endl ;
      COUT << "Examples: " << examples << endl;
      return 1;
#else
      return string2gen(string(howto)+'\n'+string(syntax)+'\n'+string(related)+'\n'+string(examples),false);
#endif
    }
    return gensizeerr(gettext("No help file found"));
    // return 0;
  }
  static const char _findhelp_s []="findhelp";
  static define_unary_function_eval_quoted (__findhelp,&_findhelp,_findhelp_s);
  define_unary_function_ptr5( at_findhelp ,alias_at_findhelp,&__findhelp,_QUOTE_ARGUMENTS,true);

  static const char _help_s []="help";
  static define_unary_function_eval_quoted (__help,&_findhelp,_help_s);
  define_unary_function_ptr5( at_help ,alias_at_help,&__help,_QUOTE_ARGUMENTS,true);

  gen _member(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    gen g=args;
    vecteur v;
    if (args.type!=_VECT){
      g=args.eval(eval_level(contextptr),contextptr);
      if (g.type!=_VECT)
	return symbolic(at_member,args);
      v=*g._VECTptr;
    }
    else {
      v=*args._VECTptr;
      if (v.size()>1){
	v[0]=eval(v[0],eval_level(contextptr),contextptr);
	v[1]=eval(v[1],eval_level(contextptr),contextptr);
      }
    }
    int s=int(v.size());
    if (s<2)
      return gentoofewargs("");
    int i=-1;
    if (v[1].type==_MAP){
      const gen_map & m=*v[1]._MAPptr;
      gen_map::const_iterator it=m.find(v[0]),itend=m.end();
      return change_subtype(it!=itend,_INT_BOOLEAN);
    }
    if (v[0].type==_STRNG && v[1].type==_STRNG){
      string f=*v[0]._STRNGptr,s=*v[1]._STRNGptr;
      int pos=s.find(f);
      if (pos<0 || pos>=s.size())
	i=0;
      else
	i=pos+1;
    }
    else {
      if (v[1].type!=_VECT)
	return gensizeerr(contextptr);
      i=equalposcomp(*v[1]._VECTptr,v[0]);
    }
    if (s==3){
      gen tmpsto;
      if (array_start(contextptr))
	tmpsto=sto(i,v[2],contextptr);
      else
	tmpsto=sto(i-1,v[2],contextptr);
      if (is_undef(tmpsto)) return tmpsto;
    }
    return i;
  }
  static const char _member_s []="member";
  static define_unary_function_eval_quoted (__member,&_member,_member_s);
  define_unary_function_ptr5( at_member ,alias_at_member,&__member,_QUOTE_ARGUMENTS,true);

  gen protectevalorevalf(const gen & g,int level,bool approx,GIAC_CONTEXT){
    gen res;
#ifdef HAVE_LIBGSL //
    gsl_set_error_handler_off();
#endif //
    ctrl_c = false; interrupted=false;
    // save cas_setup in case of an exception
    vecteur cas_setup_save = cas_setup(contextptr);
    if (cas_setup_save.size()>5 && cas_setup_save[5].type==_VECT && cas_setup_save[5]._VECTptr->size()==2){
      vecteur & v = *cas_setup_save[5]._VECTptr;
      if (is_strictly_greater(v[0],1e-6,contextptr)){
	*logptr(contextptr) << gettext("Restoring epsilon to 1e-6 from ") << v[0] << endl;
	epsilon(1e-6,contextptr);
      }
      if (is_strictly_greater(v[1],1e-6,contextptr)){
	*logptr(contextptr) << gettext("Restoring proba epsilon to 1e-6 from ") << v[0] << endl;
	proba_epsilon(contextptr)=1e-6;
      }
      cas_setup_save=cas_setup(contextptr);
    }
    debug_struct dbg;
    dbg=*debug_ptr(contextptr);
#ifndef NO_STDEXCEPT
    try {
#endif
      res=approx?g.evalf(level,contextptr):g.eval(level,contextptr);
#ifndef NO_STDEXCEPT
    }
    catch (std::runtime_error & e){
      last_evaled_argptr(contextptr)=NULL;
      *debug_ptr(contextptr)=dbg;
      res=string2gen(e.what(),false);
      res.subtype=-1;
      ctrl_c=false; interrupted=false;
      // something went wrong, so restore the old cas_setup
      cas_setup(cas_setup_save, contextptr);
    }
#endif
    return res;
  }

  gen protectevalf(const gen & g,int level, GIAC_CONTEXT){
    return protectevalorevalf(g,level,true,contextptr);
  }

  gen protecteval(const gen & g,int level, GIAC_CONTEXT){
    return protectevalorevalf(g,level,approx_mode(contextptr),contextptr);
  }

  gen _nodisp(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return string2gen("Done",false);
  }
  static const char _nodisp_s []="nodisp";
  static define_unary_function_eval (__nodisp,(const gen_op_context)_nodisp,_nodisp_s);
  define_unary_function_ptr5( at_nodisp ,alias_at_nodisp,&__nodisp,0,true);

  gen _unapply(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if ( (args.type!=_VECT) || args._VECTptr->empty() )
      return gentypeerr(contextptr);
    vecteur v=*args._VECTptr,w;
    int s=int(v.size());
    if (s<2)
      w=vecteur(1,vx_var());
    else {
      if (s==2 && v[1].type==_VECT)
	w=*v[1]._VECTptr;
      else
	w=vecteur(v.begin()+1,v.end());
    }
    gen g=subst(v[0].eval(eval_level(contextptr),contextptr),w,w,false,contextptr);
    if (g.type==_VECT && !g.subtype)
      g=makevecteur(g);
    return symb_prog3(gen(w,_SEQ__VECT),w*zero,g);
  }
  static const char _unapply_s []="unapply";
  static define_unary_function_eval_quoted (__unapply,&_unapply,_unapply_s);
  define_unary_function_ptr5( at_unapply ,alias_at_unapply,&__unapply,_QUOTE_ARGUMENTS,true);

  gen _makevector(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return vecteur(1,args);
    vecteur & v=*args._VECTptr;
    if (ckmatrix(args))
      return gen(v,_MATRIX__VECT);
    return v;
  }
  static const char _makevector_s []="makevector";
  static define_unary_function_eval (__makevector,&_makevector,_makevector_s);
  define_unary_function_ptr5( at_makevector ,alias_at_makevector,&__makevector,0,true);


  gen _makesuite(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return vecteur(1,args);
    vecteur & v=*args._VECTptr;
    return gen(v,_SEQ__VECT);
  }
  static const char _makesuite_s []="makesuite";
  static define_unary_function_eval (__makesuite,&_makesuite,_makesuite_s);
  define_unary_function_ptr5( at_makesuite ,alias_at_makesuite,&__makesuite,0,true);

  gen _matrix(const gen & g,const context * contextptr){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    if (g.type!=_VECT)
      return gentypeerr(contextptr);
    vecteur v=*g._VECTptr;
    if (ckmatrix(v))
      return gen(v,_MATRIX__VECT);
    int vs=int(v.size());
    if (vs<2)
      return gentypeerr(contextptr);
    if (vs==2 && v[0].type==_INT_ && v[1].type==_VECT){
      int l=giacmax(v[0].val,0);
      vecteur res(l);
      vecteur w(*v[1]._VECTptr);
      if (ckmatrix(w))
	aplatir(*v[1]._VECTptr,w);
      int s=giacmin(l,int(w.size()));
      for (int i=0;i<s;++i)
	res[i]=w[i];
      return gen(res,_MATRIX__VECT);
    }
    if (vs==2){
      v.push_back(zero);
      ++vs;
    }
    if ( (v[0].type!=_INT_) || (v[1].type!=_INT_) )
      return gensizeerr(contextptr);
    int l(giacmax(v[0].val,1)),c(giacmax(v[1].val,1));
    if (l*longlong(c)>LIST_SIZE_LIMIT)
      return gendimerr(contextptr);
    if (vs==3 && v[2].type<=_IDNT){
      vecteur res(l);
      for (int i=0;i<l;++i)
	res[i]=vecteur(c,v[2]);
      return gen(res,_MATRIX__VECT);
    }
    bool transpose=(vs>3);
    if (transpose){ // try to merge arguments there
      // v[2]..v[vs-1] represents flattened submatrices 
      vecteur v2;
      for (int i=2;i<vs;++i){
	if (v[i].type!=_VECT)
	  return gentypeerr(contextptr);
	vecteur & w = *v[i]._VECTptr;
	int vis=int(w.size());
	if (vis % l)
	  return gendimerr(contextptr);
	int nc=vis/l;
	for (int J=0;J<nc;++J){
	  for (int I=J;I<vis;I+=nc)
	    v2.push_back(w[I]);
	}
      }
      v[2]=v2;
      swapint(l,c);
    }
    if (v[2].type==_VECT){
      vecteur w=*v[2]._VECTptr;
      int s=int(w.size());
      if (ckmatrix(w)){
	int ss=0;
	if (s)
	  ss=int(w[0]._VECTptr->size());
	int ll=giacmin(l,s);
	for (int i=0;i<ll;++i){
	  if (ss<c)
	    w[i]=mergevecteur(*w[i]._VECTptr,vecteur(c-ss));
	  else
	    w[i]=vecteur(w[i]._VECTptr->begin(),w[i]._VECTptr->begin()+c);
	}
	if (s<l)
	  w=mergevecteur(w,vecteur(l-s,vecteur(c)));
	else
	  w=vecteur(w.begin(),w.begin()+l);
	return gen(makefreematrice(w),_MATRIX__VECT);
      }
      else {
	vecteur res;
	if (s<l*c)
	  w=mergevecteur(w,vecteur(l*c-s));
	for (int i=0;i<l;++i)
	  res.push_back(vecteur(w.begin()+i*c,w.begin()+(i+1)*c));
	if (transpose)
	  res=mtran(res);
	return gen(makefreematrice(res),_MATRIX__VECT);
      }
    }
    // v[2] as a function, should take 2 args
    gen f=v[2];
    if (!f.is_symb_of_sommet(at_program))
      return gen(vecteur(l,vecteur(c,f)),_MATRIX__VECT);
    vecteur res(l);
    int decal=array_start(contextptr); //(xcas_mode(contextptr)!=0);
    for (int i=decal;i<l+decal;++i){
      vecteur tmp(c);
      for (int j=decal;j<c+decal;++j)
	tmp[j-decal]=f(makesequence(i,j),contextptr);
      res[i-decal]=tmp;
    }
    return gen(res,_MATRIX__VECT);
  }
  static const char _matrix_s []="matrix";
  static define_unary_function_eval (__matrix,&_matrix,_matrix_s);
  define_unary_function_ptr5( at_matrix ,alias_at_matrix,&__matrix,0,true);

  static string printasbreak(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    return sommetstr;
  }
  gen _break(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return symbolic(at_break,0);
  }
  static const char _break_s []="break";
  static define_unary_function_eval2_index (104,__break,&_break,_break_s,&printasbreak);
  define_unary_function_ptr5( at_break ,alias_at_break ,&__break,0,T_BREAK);

  gen _continue(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return symbolic(at_continue,0);
  }
  static const char _continue_s []="continue";
  static define_unary_function_eval2_index (106,__continue,&_continue,_continue_s,&printasbreak);
  define_unary_function_ptr( at_continue ,alias_at_continue ,&__continue);

  //static string printaslabel(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){ return sommetstr+(' '+feuille.print(contextptr)); }
  gen _label(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return symbolic(at_label,args);
  }
  static const char _label_s []="label";
  static define_unary_function_eval2 (__label,&_label,_label_s,&printastifunction);
  define_unary_function_ptr5( at_label ,alias_at_label ,&__label,0,T_RETURN);

  gen _goto(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return symbolic(at_goto,args);
  }
  static const char _goto_s []="goto";
  static define_unary_function_eval2 (__goto,&_goto,_goto_s,&printastifunction);
  define_unary_function_ptr5( at_goto ,alias_at_goto ,&__goto,0,T_RETURN);

  /*
  static vecteur local_vars(const vecteur & v,GIAC_CONTEXT){
    const_iterateur it=v.begin(),itend=v.end();
    vecteur res;
    for (;it!=itend;++it){
      if (it->type==_IDNT && 
	  (contextptr?contextptr->tabptr->find(*it->_IDNTptr->name)==contextptr->tabptr->end():(!it->_IDNTptr->localvalue || it->_IDNTptr->localvalue->empty()))
	  )
	res.push_back(*it);
    }
    return res;
  }
  */
  static string printastilocal(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if ( (feuille.type!=_VECT) || (feuille._VECTptr->size()!=2) )
      return gettext("invalid |");
    return string("(")+feuille._VECTptr->front().print(contextptr)+string("|")+feuille._VECTptr->back().print(contextptr)+')';
  }
  gen _tilocal(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT || args._VECTptr->size()!=2)
      return symbolic(at_tilocal,args);
    vecteur & v=*args._VECTptr;
    if (v[0].type==_INT_ && v[1].type==_INT_)
      return (v[0].val | v[1].val);
    if (is_equal(v.front()))
      return symb_equal(_tilocal(makesequence((v.front()._SYMBptr->feuille)[0],v.back()),contextptr),_tilocal(makesequence((v.front()._SYMBptr->feuille)[1],v.back()),contextptr));
    // find local variables
    vecteur cond(gen2vecteur(v[1]));
    vecteur docond,vars;
    const_iterateur it=cond.begin(),itend=cond.end();
    for (;it!=itend;++it){
      if (it->type!=_SYMB)
	continue;
      unary_function_ptr & u=it->_SYMBptr->sommet;
      gen & g=it->_SYMBptr->feuille;
      if ( (g.type!=_VECT) || (g._VECTptr->empty()) )
	return gensizeerr(contextptr);
      if (u==at_equal || u==at_equal2){
	gen tmp=g._VECTptr->front(),tmp2=g._VECTptr->back();
	if (tmp.type==_IDNT){
	  gen tmp1(eval(tmp,eval_level(contextptr),contextptr));
	  if (tmp1.type==_IDNT)
	    tmp=tmp1;
	  tmp.subtype=0; // otherwise if inside a folder sto will affect tmp!
	  vars.push_back(tmp);
	  tmp2=subst(tmp2,tmp,symb_quote(tmp),false,contextptr);
	}
	docond.push_back(symbolic(at_sto,makesequence(tmp2,tmp)));
	continue;
      }
      if (u==at_sto){
	if (g._VECTptr->back().type==_IDNT)
	  vars.push_back(g._VECTptr->back());
	docond.push_back(*it);
	continue;
      }
      gen gf=g._VECTptr->front();
      if (gf.type==_IDNT)
	vars.push_back(gf);
      if (gf.type==_SYMB && gf._SYMBptr->feuille.type==_VECT && gf._SYMBptr->feuille._VECTptr->size()==2 && gf._SYMBptr->feuille._VECTptr->front().type==_IDNT)
	vars.push_back(gf._SYMBptr->feuille._VECTptr->front());
      if (it->type==_SYMB && it->_SYMBptr->sommet!=at_superieur_strict && it->_SYMBptr->sommet!=at_superieur_egal && it->_SYMBptr->sommet!=at_inferieur_strict && it->_SYMBptr->sommet!=at_inferieur_egal &&it->_SYMBptr->sommet!=at_and)
	return gensizeerr(gettext("Invalid |"));
      docond.push_back(symbolic(at_assume,*it));
    }
    vecteur v0(vars.size(),zero);
    gen gv(v[0]);
    // Replace v[0] by its value if it is a global identifier
    if (gv.type==_IDNT){
      if (contextptr){
	sym_tab::const_iterator it=contextptr->tabptr->find(gv._IDNTptr->id_name),itend=contextptr->tabptr->end();
	if (it!=itend)
	  gv=it->second;
      }
      else {
	if (gv._IDNTptr->value)
	  gv=*gv._IDNTptr->value;
      }
    }
    /*
    // Replace local variables by their value in gv
    vecteur vname(*_lname(gv,contextptr)._VECTptr),docondvar(*_lname(docond,contextptr)._VECTptr);
    vecteur vval(vname);
    iterateur jt=vval.begin(),jtend=vval.end();
    for (;jt!=jtend;++jt){
      if (jt->type!=_IDNT || equalposcomp(docondvar,*jt))
	continue;
      if (contextptr){
	sym_tab::const_iterator kt=contextptr->tabptr->find(*jt->_IDNTptr->name);
	if (kt!=contextptr->tabptr->end())
	  *jt=kt->second;
      }
      else {
	if (jt->_IDNTptr->localvalue && !jt->_IDNTptr->localvalue->empty())
	  *jt=jt->_IDNTptr->localvalue->back();
      }
    }
    gv=quotesubst(gv,vname,vval,contextptr);
    */
    // Replace vars global IDNT by local IDNT
    vecteur vname=vars;
    iterateur jt=vname.begin(),jtend=vname.end();
    for (;jt!=jtend;++jt)
      jt->subtype=_GLOBAL__EVAL;
    vecteur vval=vars;
    jt=vval.begin(),jtend=vval.end();
    for (;jt!=jtend;++jt)
      jt->subtype=0;
    gv=quotesubst(gv,vname,vval,contextptr);
    docond=*quotesubst(docond,vname,vval,contextptr)._VECTptr;
    gen prg=symb_program(gen(vname,_SEQ__VECT),gen(v0,_SEQ__VECT),symb_bloc(makevecteur(docond,gv)),contextptr);
    return prg(gen(v0,_SEQ__VECT),contextptr);
  }
  static const char _tilocal_s []="|";
  static define_unary_function_eval4_index (103,__tilocal,&_tilocal,_tilocal_s,&printastilocal,&texprintsommetasoperator);
  define_unary_function_ptr5( at_tilocal ,alias_at_tilocal,&__tilocal,_QUOTE_ARGUMENTS,0);

  string printastifunction(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if (feuille.type==_VECT && feuille.subtype==_SEQ__VECT){
      if (feuille._VECTptr->empty())
	return string(sommetstr)+" ";
      else
	return sommetstr+(" ("+feuille.print(contextptr)+')');
    }
    return sommetstr+(" "+feuille.print(contextptr));
  }

  gen _expr(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_VECT && args._VECTptr->size()==2 && args._VECTptr->front().type==_STRNG && args._VECTptr->back().type==_INT_){
      int mode=args._VECTptr->back().val;
      bool rpnmode=mode<0;
      mode=absint(mode) % 256;
      if (mode>3)
	return gensizeerr(contextptr);
      int save_mode=xcas_mode(contextptr);
      bool save_rpnmode=rpn_mode(contextptr);
      xcas_mode(contextptr)=mode;
      rpn_mode(contextptr)=rpnmode;
      gen res=eval(gen(*args._VECTptr->front()._STRNGptr,contextptr),eval_level(contextptr),contextptr);
      xcas_mode(contextptr)=save_mode;
      rpn_mode(contextptr)=save_rpnmode;
      return res;
    }
    if (args.type==_VECT && args._VECTptr->size()==2 && args._VECTptr->front().type==_STRNG && args._VECTptr->back()==at_quote)
      return gen(*args._VECTptr->front()._STRNGptr,contextptr);
    if (args.type==_VECT && !args._VECTptr->empty() && args._VECTptr->front().type==_FUNC){
      vecteur v(args._VECTptr->begin()+1,args._VECTptr->end());
      return symbolic(*args._VECTptr->front()._FUNCptr,gen(v,_SEQ__VECT));
    }
    if (args.type!=_STRNG)
      return symbolic(at_expr,args);
    return eval(gen(*args._STRNGptr,contextptr),eval_level(contextptr),contextptr);
  }
  static const char _expr_s []="expr";
  static define_unary_function_eval (__expr,&_expr,_expr_s);
  define_unary_function_ptr5( at_expr ,alias_at_expr,&__expr,0,true);

  static const char _execute_s []="execute";
  static define_unary_function_eval (__execute,&_expr,_execute_s);
  define_unary_function_ptr5( at_execute ,alias_at_execute,&__execute,0,true);

  gen _string(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    int maxp=MAX_PRINTABLE_ZINT;
    MAX_PRINTABLE_ZINT=1000000;
    string res;
    if (args.type==_VECT && args.subtype==_SEQ__VECT){
      const_iterateur it=args._VECTptr->begin(),itend=args._VECTptr->end();
      for (;it!=itend;){
	if (it->type!=_STRNG){
	  res += it->print(contextptr);
	  ++it;
	  if (it!=itend)
	    res += ','; 
	  continue;
	}
	res += *it->_STRNGptr;
	++it;
	if (it==itend)
	  return string2gen(res,false);
	if (it->type==_STRNG)
	  res += '\n';
      }
    }
    else
      res=args.print(contextptr);
    MAX_PRINTABLE_ZINT=maxp;
    return string2gen(res,false);
  }
  static const char _string_s []="string";
  static define_unary_function_eval (__string,&_string,_string_s);
  define_unary_function_ptr5( at_string ,alias_at_string,&__string,0,true);

  static const char _str_s []="str";
  static define_unary_function_eval (__str,&_string,_str_s);
  define_unary_function_ptr5( at_str ,alias_at_str,&__str,0,true);

  gen _Dialog(const gen & args,GIAC_CONTEXT){
    return undef;
  }
  static const char _Dialog_s []="Dialog";
  static define_unary_function_eval2_index (89,__Dialog,&_Dialog,_Dialog_s,&printastifunction);
  define_unary_function_ptr5( at_Dialog ,alias_at_Dialog,&__Dialog,_QUOTE_ARGUMENTS,0);

  gen _part(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if ( (args.type==_VECT) && args._VECTptr->size()==2 ){
      gen & i=args._VECTptr->back();
      gen & g=args._VECTptr->front();
      if (i.type!=_INT_ || i.val<=0){
	if (g.type!=_SYMB)
	  return string2gen(g.print(contextptr),false);
	else
	  return string2gen(g._SYMBptr->sommet.ptr()->s,false);
      }
      else {
	if (g.type!=_SYMB){
	  if (i.val!=1)
	    return gensizeerr(contextptr);
	  return g;
	}
	else {
	  vecteur v(gen2vecteur(g._SYMBptr->feuille));
	  if (signed(v.size())<i.val)
	    return gensizeerr(contextptr);
	  return v[i.val-1];
	}
      }
    }
    if (args.type==_SYMB)
      return int(gen2vecteur(args._SYMBptr->feuille).size());
    return 0;
  }
  static const char _part_s []="part";
  static define_unary_function_eval (__part,&_part,_part_s);
  define_unary_function_ptr5( at_part ,alias_at_part,&__part,0,true);

  gen _Pause(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen g1=g;
    if (is_integer(g1) || g1.type==_REAL)
      g1=evalf_double(g,1,contextptr);
    if (g1.type==_DOUBLE_){
      wait_1ms(g1._DOUBLE_val*10); // does not work on emulator
    }
    return 0;
  }
  static const char _sleep_s []="sleep";
  static define_unary_function_eval (__sleep,&_Pause,_sleep_s);
  define_unary_function_ptr5( at_sleep ,alias_at_sleep,&__sleep,0,true);

  gen _monotonic(const gen & g,GIAC_CONTEXT){
    unsigned int i=0;
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    //return clock();
    return ++i;//RTC_GetTicks();
  }
  static const char _monotonic_s []="monotonic";
  static define_unary_function_eval (__monotonic,&_monotonic,_monotonic_s);
  define_unary_function_ptr5( at_monotonic ,alias_at_monotonic,&__monotonic,0,true);

  static const char _del_s []="del";
  static define_unary_function_eval2_quoted (__del,&_purge,_del_s,&printastifunction);
  define_unary_function_ptr5( at_del ,alias_at_del,&__del,_QUOTE_ARGUMENTS,T_RETURN);

  gen matrix_apply(const gen & a,const gen & b,gen (* f) (const gen &, const gen &) ){
    if (a.type!=_VECT || b.type!=_VECT || a._VECTptr->size()!=b._VECTptr->size())
      return apply(a,b,f);
    const_iterateur it=a._VECTptr->begin(),itend=a._VECTptr->end(),jt=b._VECTptr->begin();
    vecteur res;
    res.reserve(itend-it);
    for (;it!=itend;++it,++jt){
      res.push_back(apply(*it,*jt,f));
    }
    return gen(res,a.subtype);
  }
  gen matrix_apply(const gen & a,const gen & b,GIAC_CONTEXT,gen (* f) (const gen &, const gen &,GIAC_CONTEXT) ){
    if (a.type!=_VECT || b.type!=_VECT || a._VECTptr->size()!=b._VECTptr->size())
      return apply(a,b,contextptr,f);
    const_iterateur it=a._VECTptr->begin(),itend=a._VECTptr->end(),jt=b._VECTptr->begin();
    vecteur res;
    res.reserve(itend-it);
    for (;it!=itend;++it,++jt){
      res.push_back(apply(*it,*jt,contextptr,f));
    }
    return gen(res,a.subtype);
  }
  gen prod(const gen & a,const gen &b){
    return a*b;
  }
  gen somme(const gen & a,const gen &b){
    return a+b;
  }
  gen _pointprod(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen a,b;
    if (!check_binary(g,a,b))
      return a;
    return matrix_apply(a,b,contextptr,operator_times);
  }
  static const char _pointprod_s []=".*";
  static define_unary_function_eval4_index (92,__pointprod,&_pointprod,_pointprod_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_pointprod ,alias_at_pointprod ,&__pointprod);

  gen pointdivision(const gen & a,const gen &b,GIAC_CONTEXT){
    if (a.type==_VECT && b.type!=_VECT)
      return apply1st(a,b,contextptr,pointdivision);
    if (a.type!=_VECT && b.type==_VECT)
      return apply2nd(a,b,contextptr,pointdivision);
    //return a/b;
    // reactivated 22 oct 15 for [[1,2],[3,4]] ./ [[3,4],[5,6]]
    return matrix_apply(a,b,contextptr,rdiv);
  }
  gen _pointdivision(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen a,b;
    if (!check_binary(g,a,b))
      return a;
    return pointdivision(a,b,contextptr);
  }
  static const char _pointdivision_s []="./";
  static define_unary_function_eval4_index (94,__pointdivision,&_pointdivision,_pointdivision_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_pointdivision ,alias_at_pointdivision ,&__pointdivision);

  gen giac_pow(const gen &,const gen &,GIAC_CONTEXT);
  gen pointpow(const gen & a,const gen &b,GIAC_CONTEXT){
    if (b.type!=_VECT && a.type==_VECT){
      return apply(a,b,contextptr,pointpow);
    }
    return matrix_apply(a,b,contextptr,giac_pow);
  }
  gen _pointpow(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen a,b;
    if (!check_binary(g,a,b))
      return a;
    return pointpow(a,b,contextptr);
  }
  static const char _pointpow_s []=".^";
  static define_unary_function_eval4_index (96,__pointpow,&_pointpow,_pointpow_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_pointpow ,alias_at_pointpow ,&__pointpow);

  string printassuffix(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    return feuille.print(contextptr)+sommetstr;
  }  
  gen _pourcent(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    return rdiv(g,100,contextptr);
  }
  static const char _pourcent_s []="%";
  static define_unary_function_eval2_index (100,__pourcent,&_pourcent,_pourcent_s,&printassuffix);
  define_unary_function_ptr( at_pourcent ,alias_at_pourcent ,&__pourcent);

  gen _hash(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    if (g.type!=_STRNG)
      return g;
    return gen(*g._STRNGptr,contextptr);
  }
  static const char _hash_s []="#";
  static define_unary_function_eval_index (98,__hash,&_hash,_hash_s);
  define_unary_function_ptr( at_hash ,alias_at_hash ,&__hash);

  bool user_screen=false;
  int user_screen_io_x=0,user_screen_io_y=0;
  int user_screen_fontsize=14;


  gen maple_lib(const gen &,GIAC_CONTEXT){
    return undef;
  }
  static const char _maple_lib_s[]="maple_lib";
#if defined RTOS_THREADX || defined NSPIRE || defined FXCG
  static define_unary_function_eval2_index(110,__maple_lib,&maple_lib,_maple_lib_s,&printasexp);
#else
  const unary_function_eval __maple_lib(110,&maple_lib,_maple_lib_s,&printasexp);
#endif
  define_unary_function_ptr( at_maple_lib ,alias_at_maple_lib ,&__maple_lib);


  gen _regroup(const gen &g,GIAC_CONTEXT){
    return _simplifier(g,contextptr);
  }
  static const char _regrouper_s []="regroup";
  static define_unary_function_eval (__regrouper,&_regroup,_regrouper_s);
  define_unary_function_ptr5( at_regrouper ,alias_at_regrouper,&__regrouper,0,true);

  gen find_or_make_symbol(const string & s,bool check38,GIAC_CONTEXT){
    gen tmp;
    find_or_make_symbol(s,tmp,0,check38,contextptr);
    return tmp;
  }


  static gen unit(const gen & g,GIAC_CONTEXT){
    return symbolic(at_unit,g);
  }
  static const char _unit_s []="_";
  static define_unary_function_eval_index (112,__unit,&unit,_unit_s);
  define_unary_function_ptr( at_unit ,alias_at_unit ,&__unit);

  
#ifndef FXCG
  const unary_function_ptr * binary_op_tab(){
    static const unary_function_ptr binary_op_tab_ptr []={*at_plus,*at_prod,*at_pow,*at_and,*at_ou,*at_xor,*at_different,*at_same,*at_equal,*at_unit,*at_compose,*at_composepow,*at_deuxpoints,*at_tilocal,*at_pointprod,*at_pointdivision,*at_pointpow,*at_division,*at_normalmod,*at_minus,*at_intersect,*at_union,*at_interval,*at_inferieur_egal,*at_inferieur_strict,*at_superieur_egal,*at_superieur_strict,*at_equal2,0};
    return binary_op_tab_ptr;
  }
  // unary_function_ptr binary_op_tab[]={at_and,at_ou,at_different,at_same,0};
#endif
  
  // Physical constants -> in input_lexer.ll

  gen maple_root(const gen & g,GIAC_CONTEXT){
    if (g.type!=_VECT || g._VECTptr->size()!=2)
      return symbolic(at_maple_root,g);
    vecteur & v=*g._VECTptr;
    return pow(v[1],inv(v[0],contextptr),contextptr);
  }
  static const char _maple_root_s []="root";
#if defined RTOS_THREADX || defined NSPIRE || defined FXCG
  static define_unary_function_eval(__maple_root,&maple_root,_maple_root_s);
#else
  static const unary_function_eval __maple_root(0,&maple_root,_maple_root_s);
#endif
  define_unary_function_ptr( at_maple_root ,alias_at_maple_root ,&__maple_root);

  gen symb_interrogation(const gen & e1,const gen & e3){
    if (e3.is_symb_of_sommet(at_deuxpoints)){
      gen & f =e3._SYMBptr->feuille;
      if (f.type==_VECT && f._VECTptr->size()==2)
	return symb_when(e1,f._VECTptr->front(),f._VECTptr->back());
    }
    return symb_when(e1,e3,undef);
  }

  bool first_ascend_sort(const gen & a,const gen & b){
    gen g=inferieur_strict(a[0],b[0],context0); 
    if (g.type!=_INT_)
      return a[0].islesscomplexthan(b[0]);
    return g.val==1;
  }
  bool first_descend_sort(const gen & a,const gen & b){
    gen g=superieur_strict(a[0],b[0],context0); 
    if (g.type!=_INT_)
      return !a[0].islesscomplexthan(b[0]);
    return g.val==1;
  }

  static string printaspiecewise(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    // if ( feuille.type!=_VECT || feuille._VECTptr->empty() || abs_calc_mode(contextptr)!=38)
      return string(sommetstr)+('('+feuille.print(contextptr)+')');
#if 0
    vecteur & v = *feuille._VECTptr;
    string res("CASE");
    int s=int(v.size());
    for (int i=0;i<s/2;i++){
      res += " IF ";
      res += v[2*i].print(contextptr);
      res += " THEN ";
      res += printasinnerbloc(v[2*i+1],contextptr);
      res += " END";
    }
    if (s%2){
      res += " DEFAULT ";
      res += printasinnerbloc(v[s-1],contextptr);
    }
    return res+" END";
#endif
  }
  gen _piecewise(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    // evaluate couples of condition/expression, like in a case
    if (g.type!=_VECT)
      return g;
    vecteur & v =*g._VECTptr;
    int s=int(v.size());
    gen test;
    for (int i=0;i<s/2;++i){
      test=v[2*i];
      test=equaltosame(test.eval(eval_level(contextptr),contextptr)).eval(eval_level(contextptr),contextptr);
      test=test.evalf_double(eval_level(contextptr),contextptr);
      if ( (test.type!=_DOUBLE_) && (test.type!=_CPLX) )
	return symbolic(at_piecewise,g.eval(eval_level(contextptr),contextptr));
      if (is_zero(test))
	continue;
      return v[2*i+1].eval(eval_level(contextptr),contextptr);
    }
    if (s%2)
      return v[s-1].eval(eval_level(contextptr),contextptr);
    return undeferr(gettext("No case applies"));
  }
  static const char _piecewise_s []="piecewise";
  static define_unary_function_eval2_quoted (__piecewise,&_piecewise,_piecewise_s,&printaspiecewise);
  define_unary_function_ptr5( at_piecewise ,alias_at_piecewise,&__piecewise,_QUOTE_ARGUMENTS,true);


  std::string print_program_syntax(int maple_mode){
    string logs;
    switch (maple_mode){
    case 0:
      logs="xcas";
      break;
    case 1:
      logs="maple";
      break;
    case 2:
      logs="mupad";
      break;
    case 3:
      logs="ti";
      break;
    default:
      logs=print_INT_(maple_mode);
    }
    return logs;
  }

  gen whentopiecewise(const gen & g,GIAC_CONTEXT){
    return symbolic(at_piecewise,g);
  }
  const alias_type when_tab_alias[]={(alias_type)&__when,0};
  const unary_function_ptr * const when_tab=(const unary_function_ptr * const)when_tab_alias;
  const gen_op_context when2piecewise_tab[]={whentopiecewise,0};
  gen when2piecewise(const gen & g,GIAC_CONTEXT){
    return subst(g,when_tab,when2piecewise_tab,false,contextptr);
    /*
    vector< gen_op_context > when2piecewise_v(1,whentopiecewise);
    vector< const unary_function_ptr *> when_v(1,at_when);
    return subst(g,when_v,when2piecewise_v,false,contextptr);
    */
  }

  gen piecewisetowhen(const gen & g,GIAC_CONTEXT){
    if (g.type!=_VECT)
      return g;
    vecteur v = *g._VECTptr;
    int s=int(v.size());
    if (s==1)
      return gensizeerr(contextptr);
    if (s==2){
      v.push_back(0); // undef does not work
      return symbolic(at_when,gen(v,_SEQ__VECT));
    }
    if (s==3)
      return symbolic(at_when,g);
    gen tmp=piecewisetowhen(vecteur(v.begin()+2,v.end()),contextptr);
    return symbolic(at_when,makesequence(v[0],v[1],tmp));
  }
  const alias_type piecewise_tab_alias[]={alias_at_piecewise,0};
  const unary_function_ptr * const piecewise_tab=(const unary_function_ptr * const)piecewise_tab_alias;
  const gen_op_context piecewise2when_tab[]={piecewisetowhen,0};
  gen piecewise2when(const gen & g,GIAC_CONTEXT){
    return subst(g,piecewise_tab,piecewise2when_tab,false,contextptr);
    /*
    vector< const unary_function_ptr *> piecewise_v(1,at_piecewise);
    vector< gen_op_context > piecewise2when_v(1,piecewisetowhen);
    return subst(g,piecewise_v,piecewise2when_v,false,contextptr);
    */
  }

  gen whentosign(const gen & g,GIAC_CONTEXT){
    if (g.type!=_VECT || g._VECTptr->size()!=3)
      return gensizeerr(contextptr);
    vecteur v = *g._VECTptr;
    if (v[0].is_symb_of_sommet(at_not))
      return when2sign(makevecteur(v[0]._SYMBptr->feuille,v[2],v[1]),contextptr);
    if (v[0].is_symb_of_sommet(at_and) && v[0]._SYMBptr->feuille.type==_VECT){
      vecteur vand=*v[0]._SYMBptr->feuille._VECTptr;
      if (vand.size()==2)
	return whentosign(makevecteur(vand[0],whentosign(makevecteur(vand[1],v[1],v[2]),contextptr),v[2]),contextptr);
      if (vand.size()>2){
	gen vandlast=vand.back();
	vand.pop_back();
	return whentosign(makevecteur(vandlast,whentosign(makevecteur(symbolic(at_and,vand),v[1],v[2]),contextptr),v[2]),contextptr);
      }
    }
    if (v[0].is_symb_of_sommet(at_ou) && v[0]._SYMBptr->feuille.type==_VECT){
      vecteur vor=*v[0]._SYMBptr->feuille._VECTptr;
      if (vor.size()==2)
	return whentosign(makevecteur(vor[0],v[1],whentosign(makevecteur(vor[1],v[1],v[2]),contextptr)),contextptr);
      if (vor.size()>2){
	gen vorlast=vor.back();
	vor.pop_back();
	return whentosign(makevecteur(vorlast,v[1],whentosign(makevecteur(symbolic(at_and,vor),v[1],v[2]),contextptr)),contextptr);
      }
    }
    if (is_equal(v[0]) || v[0].is_symb_of_sommet(at_same)){
      *logptr(contextptr) << gettext("Assuming false condition ") << v[0].print(contextptr) << endl;
      return v[2];
    }
    if (v[0].is_symb_of_sommet(at_different)){
      *logptr(contextptr) << gettext("Assuming true condition ") << v[0].print(contextptr) << endl;
      return v[1];
    }
    bool ok=false;
    if (v[0].is_symb_of_sommet(at_superieur_strict) || v[0].is_symb_of_sommet(at_superieur_egal)){
      v[0]=v[0]._SYMBptr->feuille[0]-v[0]._SYMBptr->feuille[1];
      ok=true;
    }
    if (!ok && (v[0].is_symb_of_sommet(at_inferieur_strict) || v[0].is_symb_of_sommet(at_inferieur_egal)) ){
      v[0]=v[0]._SYMBptr->feuille[1]-v[0]._SYMBptr->feuille[0];
      ok=true;
    }
    if (!ok)
      return gensizeerr(gettext("Unable to handle when condition ")+v[0].print(contextptr));
    return symbolic(at_sign,v[0])*(v[1]-v[2])/2+(v[1]+v[2])/2;
  }
  const gen_op_context when2sign_tab[]={whentosign,0};
  gen when2sign(const gen & g,GIAC_CONTEXT){
    return subst(g,when_tab,when2sign_tab,false,contextptr);
    /*
    vector< gen_op_context > when2sign_v(1,whentosign);
    vector< const unary_function_ptr *> when_v(1,at_when);
    return subst(g,when_v,when2sign_v,false,contextptr);
    */
  }

  gen iftetowhen(const gen & g,GIAC_CONTEXT){
    return symbolic(at_when,g);
  }
  const alias_type ifte_tab_alias[]={(alias_type)&__ifte,0};
  const unary_function_ptr * const ifte_tab=(const unary_function_ptr * const)ifte_tab_alias;
  const gen_op_context ifte2when_tab[]={iftetowhen,0};
  gen ifte2when(const gen & g,GIAC_CONTEXT){
    return subst(g,ifte_tab,ifte2when_tab,false,contextptr);
  }

  // test if m(i) is an array index: that will not be the case if
  // i is an _IDNT or a list of _IDNT
  // 
  bool is_array_index(const gen & m,const gen & i,GIAC_CONTEXT){
    if (i.type==_VECT){
      for (unsigned j=0;j<i._VECTptr->size();++j){
	gen g=(*i._VECTptr)[j];
	if (g.type==_IDNT || g.is_symb_of_sommet(at_equal) || g.is_symb_of_sommet(at_deuxpoints) || g.is_symb_of_sommet(at_sto))
	  continue;
	return true;
      }
    }
    else {
      if (i.type!=_IDNT)
	return true;
    }
    return false;
    // commented otherwise is_array_index inside a program would depend on the global
    // value of m
    //gen mv=eval(m,1,contextptr);
    //return mv.type==_VECT;
  }

  gen _autosimplify(const gen & g,GIAC_CONTEXT){
    if (is_zero(g) && g.type!=_VECT){
      autosimplify("Nop",contextptr);
      return 1;
    }
    if (is_one(g)){
      autosimplify("regroup",contextptr);
      return 1;
    }
    if (g==2){
      autosimplify("simplify",contextptr);
      return 1;
    }
    if (g.type!=_IDNT && g.type!=_FUNC && g.type!=_SYMB)
      return gen(autosimplify(contextptr),contextptr);
    autosimplify(g.print(contextptr),contextptr);
    return 1;
  }
  static const char _autosimplify_s []="autosimplify";
  static define_unary_function_eval (__autosimplify,&_autosimplify,_autosimplify_s);
  define_unary_function_ptr5( at_autosimplify ,alias_at_autosimplify,&__autosimplify,0,true);

  gen _struct_dot(const gen & g,GIAC_CONTEXT){
    if (g.type!=_VECT)
      return gensizeerr(contextptr);
    vecteur w=*g._VECTptr;
    size_t ws=w.size();
    if (ws!=2)
      return gensizeerr(contextptr);
    gen a=w[0],b=w[1],m;
    if (b.type==_IDNT)
      b=eval(b,1,contextptr);
    if (b.type==_SYMB){
      unary_function_ptr u=b._SYMBptr->sommet;
      const gen & f=b._SYMBptr->feuille;
      if (u==at_of && f.type==_VECT && f._VECTptr->size()==2){
	gen s=eval(f._VECTptr->front(),1,contextptr);
	if (s.type==_FUNC)
	  u=*s._FUNCptr;
      }
      if (equalposcomp(plot_sommets,u) || u==at_clear || u==at_scatterplot || u==at_diagrammebatons)
	return eval(b,1,contextptr);
    }
    if (b.type==_FUNC)
      b=symbolic(*b._FUNCptr,gen(vecteur(0),_SEQ__VECT));
    if (a.type==_IDNT){
      gen tmp=eval(a,1,contextptr);
      if (tmp==a){ // try to eval at global level
	tmp=global_eval(tmp,1);
      }
      if (tmp.type==_MAP && b.is_symb_of_sommet(at_of)){
	const gen & f =b._SYMBptr->feuille;
	if (f.type==_VECT && f._VECTptr->size()==2){
	  const vecteur & v =*f._VECTptr;
	  if (v.size()==2 && v.front().type==_IDNT && strcmp(v.front()._IDNTptr->id_name,"update")==0){
	    gen m=eval(v.back(),1,contextptr);
	    if (m.type==_MAP){
	      tmp._MAPptr->insert(m._MAPptr->begin(),m._MAPptr->end());
	      return tmp;
	    }
	  }
	}
      }
      if (tmp.type==_IDNT && (strcmp(tmp._IDNTptr->id_name,"numpy")==0 || strcmp(tmp._IDNTptr->id_name,"pylab")==0 || strcmp(tmp._IDNTptr->id_name,"matplotlib")==0)){
	if (b.type==_SYMB){
	  gen w1=eval(w[1],1,contextptr);
	  if (w1==at_float || w1==at_real)
	    return w1;
	  tmp=eval(b._SYMBptr->feuille,eval_level(contextptr),contextptr);
	  tmp=evalf_double(tmp,1,contextptr);
	  if (b.is_symb_of_sommet(at_dot))
	    return _prod(tmp,contextptr);
	  return b._SYMBptr->sommet(tmp,contextptr);
	}
	gen tmp=eval(b,1,contextptr);
	tmp=evalf_double(tmp,1,contextptr);
	return tmp;
      }
      if (tmp.type==_IDNT && 
	  (strcmp(tmp._IDNTptr->id_name,"math")==0 ||
	   strcmp(tmp._IDNTptr->id_name,"cmath")==0 ||
	   strcmp(tmp._IDNTptr->id_name,"kandinsky")==0)
	  ){
	if (b.type==_SYMB){
	  tmp=eval(b._SYMBptr->feuille,eval_level(contextptr),contextptr);
	  return b._SYMBptr->sommet(tmp,contextptr);
	}
      }
      if (ckmatrix(tmp)){
	if (w[1].type==_IDNT){
	  const char * ch =w[1]._IDNTptr->id_name;
	  if (ch){
	    if (ch[0]=='T')
	      return mtran(*tmp._VECTptr);
	    if (ch[0]=='H')
	      return _trn(tmp,contextptr);
	    if (ch[0]=='I')
	      return minv(*tmp._VECTptr,contextptr);
	  }
	}
	if (w[1].is_symb_of_sommet(at_of))
	  m=tmp;
      }
      else {
	if (tmp.type==_VECT && w[1].type==_IDNT){
	  const char * ch =w[1]._IDNTptr->id_name;
	  if (ch){
	    if (ch[0]=='T')
	      return _tran(tmp,contextptr);
	  }
	}
      }
      if (tmp.type==_VECT && b.type==_SYMB){
	// check tmp size, workaround for progs with l:=[]; l.append(1);
	// where the code would self modify itself
	// disabled, sto should now do a copy of constant lists
	if (b._SYMBptr->sommet==at_append 
	    //&& tmp._VECTptr->size()>=8
	    ){
	  tmp._VECTptr->push_back(eval(b._SYMBptr->feuille,eval_level(contextptr),contextptr));
	  return tmp;
	}
	if ( (b._SYMBptr->sommet==at_suppress || b._SYMBptr->sommet==at_clear) && b._SYMBptr->feuille.type==_VECT && b._SYMBptr->feuille._VECTptr->empty()){
	  tmp._VECTptr->clear();
	  return tmp;
	}
      }
      if (tmp.type==_FUNC)
	a=tmp;
    }
    if (b.type!=_SYMB)
      return _prod(eval(g,eval_level(contextptr),contextptr),contextptr);
    gen f=b;
    unary_function_ptr u=b._SYMBptr->sommet;
    bool cmd=false;
    if (a!=at_random){
      f=b._SYMBptr->feuille;
      if (u==at_of && f.type==_VECT && f._VECTptr->size()==2){
	gen fn=eval(f._VECTptr->front(),1,contextptr);
	if (fn.type==_FUNC){
	  cmd=true;
	  f=f._VECTptr->back();
	  u=*fn._FUNCptr;
	  b=symbolic(u,f);
	}
      }
      vecteur v(1,f);
      if (f.type==_VECT && f.subtype==_SEQ__VECT)
	v=*f._VECTptr;
      if (v.empty())
	f=eval(a,1,contextptr);
      else {
	if (u==at_remove)
	  v.push_back(a);
	else {
	  if (!cmd && ckmatrix(m) && v.front().type==_IDNT){ // ex: m.reshape(2,4)
	    b=v.front();
	    v.front()=m;
	    v=makevecteur(b,gen(v,_SEQ__VECT));
	  }
	  else {
	    if (u==at_of && v.size()==2){
	      if (v.back().type==_VECT){
		vecteur w=*v.back()._VECTptr;
		w.insert(w.begin(),a);
		v=makevecteur(v.front(),gen(w,_SEQ__VECT));
	      }
	      else
		v=makevecteur(v.front(),makesequence(a,v.back()));
	    }
	    else
	      v.insert(v.begin(),a);
	  }
	}
	f=gen(v,f.type==_VECT?f.subtype:_SEQ__VECT);
      }
      f=symbolic(u,f);
    }
    f=eval(f,eval_level(contextptr),contextptr);
    if ((a.type==_IDNT || a.type==_SYMB) && (u==at_revlist || u==at_reverse || u==at_sort || u==at_append || u==at_prepend || u==at_concat || u==at_extend || u==at_rotate || u==at_shift || u==at_suppress || u==at_remove || u==at_insert ))
      return sto(f,a,contextptr);
    return f;
  }
  static const char _struct_dot_s []=".";
  static define_unary_function_eval4_index (175,__struct_dot,&_struct_dot,_struct_dot_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr5( at_struct_dot ,alias_at_struct_dot,&__struct_dot,_QUOTE_ARGUMENTS,true);

  gen _giac_assert(const gen & args,GIAC_CONTEXT){
    gen test=args;
    string msg(gettext("assert failure: ")+args.print(contextptr));
    if (args.type==_VECT && args.subtype==_SEQ__VECT && args._VECTptr->size()==2){
      test=args._VECTptr->back();
      if (test.type==_STRNG) msg=*test._STRNGptr; else msg=test.print(contextptr);
      test=args._VECTptr->front();
    }
    test=equaltosame(test);
    int evallevel=eval_level(contextptr);
    test=equaltosame(test).eval(evallevel,contextptr);
    if (!is_integer(test))
      test=test.evalf_double(evallevel,contextptr);
    if (!is_integral(test) || test.val!=1)
      return gensizeerr(msg);
    return 1;
  }
  static const char _giac_assert_s []="assert";
  static define_unary_function_eval (__giac_assert,&_giac_assert,_giac_assert_s);
  define_unary_function_ptr5( at_giac_assert ,alias_at_giac_assert,&__giac_assert,_QUOTE_ARGUMENTS,T_RETURN);

  gen _index(const gen & args,GIAC_CONTEXT){
    if (args.type!=_VECT || args._VECTptr->size()!=2)
      return gensizeerr(contextptr);
    gen l;
    if (args._VECTptr->front().type==_STRNG)
      l=_find(args,contextptr);
    else
      l=_find(makesequence(args._VECTptr->back(),args._VECTptr->front()),contextptr);
    if (l.type!=_VECT)
      return l;
    if (l._VECTptr->empty())
      return gensizeerr(contextptr);
    return l._VECTptr->front();
  }
  static const char _index_s []="index";
  static define_unary_function_eval (__index,&_index,_index_s);
  define_unary_function_ptr5( at_index ,alias_at_index,&__index,0,true);

  gen _giac_bool(const gen & args,GIAC_CONTEXT){
    bool b=args.type==_VECT?args._VECTptr->empty():is_exactly_zero(args);
    gen r=b?0:1;
    r.subtype=_INT_BOOLEAN;
    return r;
  }
  static const char _giac_bool_s []="bool";
  static define_unary_function_eval (__giac_bool,&_giac_bool,_giac_bool_s);
  define_unary_function_ptr5( at_giac_bool ,alias_at_giac_bool,&__giac_bool,0,true);

  gen _giac_bin(const gen & a,GIAC_CONTEXT){
    if (a.type==_STRNG && a.subtype==-1) return  a;
    gen a_(a);
    if (!is_integral(a_) || a_.type==_ZINT)
      return gentypeerr(contextptr);
    int i=a_.val;
    string s;
    while (i){
      s = char('0'+(i%2))+s;
      i /= 2;
    }
    return string2gen("0b"+s,false);
  }
  static const char _giac_bin_s []="bin";
  static define_unary_function_eval (__giac_bin,&_giac_bin,_giac_bin_s);
  define_unary_function_ptr5( at_giac_bin ,alias_at_giac_bin,&__giac_bin,0,true);

  gen _giac_hex(const gen & a,GIAC_CONTEXT){
    if (a.type==_STRNG && a.subtype==-1) return  a;
    gen a_(a);
    if (!is_integral(a_))
      return gentypeerr(contextptr);
    string s=a_.type==_INT_?hexa_print_INT_(a_.val):hexa_print_ZINT(*a._ZINTptr);
    return string2gen(s,false);
  }
  static const char _giac_hex_s []="hex";
  static define_unary_function_eval (__giac_hex,&_giac_hex,_giac_hex_s);
  define_unary_function_ptr5( at_giac_hex ,alias_at_giac_hex,&__giac_hex,0,true);

  gen _heapify(const gen & args,GIAC_CONTEXT){
    if (args.type!=_VECT)
      return gensizeerr(contextptr);
    iterateur it=args._VECTptr->begin(),itend=args._VECTptr->end();    
    gen f=at_inferieur_strict_sort;
    if (args.type==_SEQ__VECT && itend-it==2 && it->type==_VECT){
      f=*it;
      it=f._VECTptr->begin();
      itend=f._VECTptr->end();
      f=args._VECTptr->back();
    }
    make_heap(it,itend,gen_sort(f,contextptr));
    return 1;
  }
  static const char _heapify_s []="heapify";
  static define_unary_function_eval (__heapify,&_heapify,_heapify_s);
  define_unary_function_ptr5( at_heapify ,alias_at_heapify,&__heapify,0,true);

  gen _heappop(const gen & args,GIAC_CONTEXT){
    if (args.type!=_VECT)
      return gensizeerr(contextptr);
    gen v=args;
    iterateur it=args._VECTptr->begin(),itend=args._VECTptr->end();
    gen f=at_inferieur_strict_sort;
    if (args.type==_SEQ__VECT && itend-it==2 && it->type==_VECT){
      v=*it;
      it=v._VECTptr->begin();
      itend=v._VECTptr->end();
      f=args._VECTptr->back();
    }
    if (itend==it)
      return gendimerr(contextptr);
    pop_heap(it,itend,gen_sort(f,contextptr));
    v._VECTptr->pop_back();
    return *itend;
  }
  static const char _heappop_s []="heappop";
  static define_unary_function_eval (__heappop,&_heappop,_heappop_s);
  define_unary_function_ptr5( at_heappop ,alias_at_heappop,&__heappop,0,true);

  gen _heappush(const gen & args,GIAC_CONTEXT){
    if (args.type!=_VECT)
      return gensizeerr(contextptr);
    gen f=at_inferieur_strict_sort;
    if (args.type!=_VECT || args._VECTptr->size()<2)
      return gensizeerr(contextptr);
    vecteur & v=*args._VECTptr;
    if (v.size()==3) f=v[2];
    if (v[0].type!=_VECT) return gensizeerr(contextptr);
    v[0]._VECTptr->push_back(v[1]);
    iterateur it=v[0]._VECTptr->begin(),itend=v[0]._VECTptr->end();
    push_heap(it,itend,gen_sort(f,contextptr));
    return v[0];
  }
  static const char _heappush_s []="heappush";
  static define_unary_function_eval (__heappush,&_heappush,_heappush_s);
  define_unary_function_ptr5( at_heappush ,alias_at_heappush,&__heappush,0,true);


#ifndef NO_NAMESPACE_GIAC
} // namespace giac
#endif // ndef NO_NAMESPACE_GIAC
