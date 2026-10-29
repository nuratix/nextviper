#pragma once
#include <string_view>
namespace nextviper {
// Self-contained C++ runtime embedded in each generated translation unit.
inline constexpr std::string_view NATIVE_RUNTIME_SOURCE = R"NVCPP(
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

struct NV;
struct NVFunction;
using NVArgs = std::vector<NV>;
using NVCells = std::vector<std::shared_ptr<NV>>;
struct NV {
    enum Kind { Nil, Bool, Int, Float, String, Array, Map, Function } kind = Nil;
    bool b = false; int64_t i = 0; double f = 0;
    std::string s;
    std::shared_ptr<std::vector<NV>> a;
    std::shared_ptr<std::map<std::string, NV>> m;
    std::shared_ptr<NVFunction> fn;
};
struct NVFunction { std::function<NV(const NVArgs&)> call; size_t arity; std::string name; };
[[noreturn]] static void nv_error(const std::string& message) { throw std::runtime_error(message); }
static NV nv_i(int64_t n) { NV v; v.kind=NV::Int; v.i=n; return v; }
static NV nv_f(double n) { NV v; v.kind=NV::Float; v.f=n; return v; }
static NV nv_b(bool n) { NV v; v.kind=NV::Bool; v.b=n; return v; }
static NV nv_s(std::string s) { NV v; v.kind=NV::String; v.s=std::move(s); return v; }
static NV nv_function(std::function<NV(const NVArgs&)> call, size_t arity, std::string name) {
    NV v; v.kind=NV::Function; v.fn=std::make_shared<NVFunction>(NVFunction{std::move(call),arity,std::move(name)}); return v;
}
static NV nv_checked(NV value, std::string annotation) {
    std::transform(annotation.begin(),annotation.end(),annotation.begin(),[](unsigned char c){return std::tolower(c);});
    bool matches = annotation.empty() || annotation=="any";
    if(annotation=="int")matches=value.kind==NV::Int;
    else if(annotation=="float"||annotation=="number")matches=value.kind==NV::Int||value.kind==NV::Float;
    else if(annotation=="string"||annotation=="str")matches=value.kind==NV::String;
    else if(annotation=="bool")matches=value.kind==NV::Bool;
    else if(annotation=="list"||annotation=="array"||annotation.starts_with("list["))matches=value.kind==NV::Array;
    else if(annotation=="map"||annotation=="object"||annotation.starts_with("map["))matches=value.kind==NV::Map;
    else if(annotation=="nil"||annotation=="null"||annotation=="void"||annotation=="none")matches=value.kind==NV::Nil;
    else if(!matches)return value; // Custom annotations remain gradual.
    if(!matches)nv_error("value does not match type annotation "+annotation);
    return value;
}
static bool nv_number(const NV& v) { return v.kind==NV::Int || v.kind==NV::Float; }
static double nv_num(const NV& v) {
    if (!nv_number(v)) nv_error("numeric operand required");
    return v.kind==NV::Int ? static_cast<double>(v.i) : v.f;
}
static int64_t nv_int(const NV& v) { if(v.kind!=NV::Int) nv_error("integer operand required"); return v.i; }
static bool nv_truth(const NV& v) {
    switch(v.kind) {
        case NV::Nil:return false; case NV::Bool:return v.b; case NV::Int:return v.i!=0;
        case NV::Float:return v.f!=0; case NV::String:return !v.s.empty();
        case NV::Array:return !v.a->empty(); case NV::Map:return !v.m->empty(); default:return true;
    }
}
static std::string nv_text(const NV& v, bool inspect=false, std::set<const void*>* seen=nullptr) {
    std::set<const void*> local; if(!seen) seen=&local;
    std::ostringstream out;
    switch(v.kind) {
        case NV::Nil:return "nil"; case NV::Bool:return v.b?"true":"false";
        case NV::Int:return std::to_string(v.i);
        case NV::Float: { out<<v.f; auto s=out.str(); if(s.find_first_of(".eE")==std::string::npos) s+=".0"; return s; }
        case NV::String:
            if(!inspect) return v.s;
            out << '"'; for(char c:v.s) { switch(c) { case '\\':out<<"\\\\";break;case '"':out<<"\\\"";break;
                case '\n':out<<"\\n";break;case '\r':out<<"\\r";break;case '\t':out<<"\\t";break;default:out<<c; } } out<<'"';return out.str();
        case NV::Function:return "<fn "+v.fn->name+">";
        default:break;
    }
    const void* id=v.kind==NV::Array?static_cast<const void*>(v.a.get()):static_cast<const void*>(v.m.get());
    if(seen->count(id)) return "<cycle>";
    if(seen->size()>=256) return "<depth-limit>";
    seen->insert(id);
    if(v.kind==NV::Array) { out<<'['; for(size_t j=0;j<v.a->size();++j) { if(j)out<<", ";out<<nv_text((*v.a)[j],true,seen); } out<<']'; }
    else { out<<'{';size_t j=0;for(const auto& [k,x]:*v.m) { if(j++)out<<", ";out<<nv_text(nv_s(k),true,seen)<<": "<<nv_text(x,true,seen); }out<<'}'; }
    seen->erase(id); return out.str();
}
static bool nv_equal(const NV& a,const NV& b,size_t depth=0) {
    if(a.kind==NV::Int && b.kind==NV::Int) return a.i==b.i;
    if(nv_number(a)&&nv_number(b))return nv_num(a)==nv_num(b);
    if(a.kind!=b.kind)return false;
    if(depth>=256)nv_error("collection equality nesting limit exceeded");
    switch(a.kind) {
        case NV::Nil:return true;case NV::Bool:return a.b==b.b;case NV::String:return a.s==b.s;
        case NV::Function:return a.fn==b.fn;
        case NV::Array:
            if(a.a==b.a)return true;if(a.a->size()!=b.a->size())return false;
            for(size_t j=0;j<a.a->size();++j)if(!nv_equal((*a.a)[j],(*b.a)[j],depth+1))return false;return true;
        case NV::Map:
            if(a.m==b.m)return true;if(a.m->size()!=b.m->size())return false;
            for(const auto& [k,v]:*a.m) { auto it=b.m->find(k);if(it==b.m->end()||!nv_equal(v,it->second,depth+1))return false; } return true;
        default:return false;
    }
}
static NV nv_binary(const std::string& op,const NV& a,const NV& b) {
    if(op=="eq")return nv_b(nv_equal(a,b));if(op=="ne")return nv_b(!nv_equal(a,b));
    if(op=="add" && (a.kind==NV::String||b.kind==NV::String))return nv_s(nv_text(a)+nv_text(b));
    if(op=="add" && a.kind==NV::Array && b.kind==NV::Array) {
        if(a.a->size()+b.a->size()>10000000)nv_error("array size limit exceeded");
        NV v;v.kind=NV::Array;v.a=std::make_shared<NVArgs>(*a.a);v.a->insert(v.a->end(),b.a->begin(),b.a->end());return v;
    }
    if(op=="mul" && a.kind==NV::String && b.kind==NV::Int) {
        if(b.i<=0||a.s.empty())return nv_s("");if(!a.s.empty() && static_cast<uint64_t>(b.i)>67108864/a.s.size())nv_error("string size limit exceeded");
        std::string s;for(int64_t j=0;j<b.i;++j)s+=a.s;return nv_s(std::move(s));
    }
    if(op=="lt"||op=="le"||op=="gt"||op=="ge") {
        bool less=false,equal=nv_equal(a,b);
        if(a.kind==NV::String&&b.kind==NV::String)less=a.s<b.s;
        else if(a.kind==NV::Int&&b.kind==NV::Int)less=a.i<b.i;
        else less=nv_num(a)<nv_num(b);
        return nv_b(op=="lt"?less:op=="le"?(less||equal):op=="gt"?(!less&&!equal):!less);
    }
    if(op=="mod") { auto x=nv_int(a),y=nv_int(b);if(!y)nv_error("division by zero");return nv_i(x==INT64_MIN&&y==-1?0:x%y); }
    if(op=="div") {
        double y=nv_num(b);if(y==0)nv_error("division by zero");
        if(a.kind==NV::Int&&b.kind==NV::Int) {
            if(a.i==INT64_MIN&&b.i==-1)nv_error("integer overflow");
            if(a.i%b.i==0)return nv_i(a.i/b.i);
        }
        return nv_f(nv_num(a)/y);
    }
    if(a.kind==NV::Int&&b.kind==NV::Int&&(op=="add"||op=="sub"||op=="mul")) {
        int64_t result;bool overflow=op=="add"?__builtin_add_overflow(a.i,b.i,&result):op=="sub"?__builtin_sub_overflow(a.i,b.i,&result):__builtin_mul_overflow(a.i,b.i,&result);
        if(overflow)nv_error("integer overflow");return nv_i(result);
    }
    if(op=="pow" && a.kind==NV::Int && b.kind==NV::Int && b.i>=0) {
        NV result=nv_i(1),base=a;uint64_t e=b.i;while(e) { if(e&1)result=nv_binary("mul",result,base);e>>=1;if(e)base=nv_binary("mul",base,base); }return result;
    }
    double x=nv_num(a),y=nv_num(b);
    if(op=="add")return nv_f(x+y);if(op=="sub")return nv_f(x-y);if(op=="mul")return nv_f(x*y);if(op=="pow")return nv_f(std::pow(x,y));
    nv_error("unsupported binary operation "+op);
}
static NV nv_call(const NV& function,const NVArgs& args) {
    if(function.kind!=NV::Function)nv_error("cannot call a non-function");
    if(args.size()!=function.fn->arity)nv_error("function argument count mismatch");
    static thread_local size_t depth=0;
    if(depth>=1000)nv_error("maximum call stack depth exceeded");
    struct Guard{size_t& d;Guard(size_t& x):d(x){++d;}~Guard(){--d;}} guard(depth);
    return function.fn->call(args);
}
static NV nv_builtin(std::string name,const NVArgs& args) {
    auto arity=[&](size_t n){if(args.size()!=n)nv_error(name+" argument count mismatch");};
    bool math_module = name.starts_with("math_");
    if(name.starts_with("math_"))name=name.substr(5);
    if(name=="array_new") { arity(0);NV v;v.kind=NV::Array;v.a=std::make_shared<NVArgs>();return v; }
    if(name=="object_new") { arity(0);NV v;v.kind=NV::Map;v.m=std::make_shared<std::map<std::string,NV>>();return v; }
    if(name=="print"||name=="println"||name=="io_print"||name=="print_raw") {
        for(size_t j=0;j<args.size();++j){if(j)std::cout<<' ';std::cout<<nv_text(args[j]);}if(name!="print_raw")std::cout<<'\n';return {};
    }
    if(name=="len"||name=="string_len") {
        arity(1);const auto& v=args[0];if(v.kind==NV::Array)return nv_i(v.a->size());if(v.kind==NV::Map)return nv_i(v.m->size());if(v.kind==NV::String)return nv_i(v.s.size());nv_error("len requires a collection");
    }
    if(name=="push"||name=="append"||name=="insert") {
        arity(name=="insert"?3:2);auto v=args[0];if(v.kind!=NV::Array)nv_error("array required");if(v.a->size()>=10000000)nv_error("array size limit exceeded");
        if(name=="insert") { auto i=nv_int(args[1]);if(i<0)i+=v.a->size();if(i<0||static_cast<size_t>(i)>v.a->size())nv_error("index out of bounds");v.a->insert(v.a->begin()+i,args[2]); }
        else v.a->push_back(args[1]);return v;
    }
    if(name=="get"||name=="set"||name=="set_prop") {
        arity(name=="get"?2:3);auto v=args[0];const auto& k=args[1];
        if(v.kind==NV::Map) { if(k.kind!=NV::String)nv_error("map keys must be strings");if(name=="get") { auto it=v.m->find(k.s);if(it==v.m->end())nv_error("unknown map key");return it->second; }(*v.m)[k.s]=args[2];return args[2]; }
        auto i=nv_int(k);size_t len=v.kind==NV::Array?v.a->size():v.kind==NV::String?v.s.size():0;
        if(i<0)i+=len;if(i<0||static_cast<size_t>(i)>=len)nv_error("index out of bounds");
        if(v.kind==NV::String) {if(name!="get")nv_error("strings are immutable");return nv_s(v.s.substr(i,1));}
        if(v.kind!=NV::Array)nv_error("indexing requires a collection");if(name=="get")return (*v.a)[i];(*v.a)[i]=args[2];return args[2];
    }
    if(name=="pop") { arity(1);auto v=args[0];if(v.kind!=NV::Array||v.a->empty())nv_error("pop requires a nonempty array");NV last=v.a->back();v.a->pop_back();return last; }
    if(name=="range") {
        if(args.empty()||args.size()>3)nv_error("range requires 1 to 3 arguments");int64_t start=args.size()==1?0:nv_int(args[0]),end=nv_int(args[args.size()==1?0:1]),step=args.size()==3?nv_int(args[2]):1;
        if(!step)nv_error("range step cannot be zero");NV v=nv_builtin("array_new",{});
        for(int64_t i=start;step>0?i<end:i>end;) { if(v.a->size()>=10000000)nv_error("array size limit exceeded");v.a->push_back(nv_i(i));int64_t next;if(__builtin_add_overflow(i,step,&next))break;i=next; }return v;
    }
    if(name=="clock") { arity(0);return nv_f(std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count()); }
    if(name=="time_now"||name=="time_now_ms") { arity(0);auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();return nv_i(name=="time_now"?ms/1000:ms); }
    if(name=="time_sleep") {arity(1);auto n=nv_int(args[0]);if(n<0)nv_error("negative sleep duration");std::this_thread::sleep_for(std::chrono::milliseconds(n));return {};}
    if(name=="sqrt"||name=="cbrt"||name=="floor"||name=="ceil"||name=="round"||name=="abs"||name=="sin"||name=="cos") {
        arity(1);double x=nv_num(args[0]);if(name=="sqrt"){if(x<0)nv_error("cannot take sqrt of negative number");return nv_f(std::sqrt(x));}if(name=="cbrt")return nv_f(std::cbrt(x));
        if(name=="floor")return nv_f(std::floor(x));if(name=="ceil")return nv_f(std::ceil(x));if(name=="round")return nv_f(std::round(x));
        if(name=="sin")return nv_f(std::sin(x));if(name=="cos")return nv_f(std::cos(x));
        if(args[0].kind==NV::Int) {if(args[0].i==INT64_MIN)nv_error("integer overflow");return nv_i(args[0].i<0?-args[0].i:args[0].i);}return nv_f(std::abs(x));
    }
    if(name=="pow") {arity(2);return math_module?nv_f(std::pow(nv_num(args[0]),nv_num(args[1]))):nv_binary("pow",args[0],args[1]);}
    if(name=="min"||name=="max") {arity(2);bool less=nv_truth(nv_binary("lt",args[0],args[1]));return args[(name=="min"?less:!less)?0:1];}
    if(name=="str") {arity(1);return nv_s(nv_text(args[0]));}
    if(name=="assert") {if(args.empty()||args.size()>2)nv_error("assert argument count mismatch");if(!nv_truth(args[0]))nv_error(args.size()==2?nv_text(args[1]):"assertion failed");return nv_b(true);}
    nv_error("native builtin not supported: "+name);
}
)NVCPP";
}
