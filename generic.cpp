// naming:
// suffix "H" stands for "Helper"
// "D" stands for "Derived"
// "o"/"O" means the main value of a struct / whatever context
#pragma once
#include<iostream>
#include<string>
#include<chrono>
#include<array>
#include<thread>
#include<typeinfo> // for getting type names
#include<cxxabi.h> // for demangling type names
#include<atomic>
#include<cstdint>
#define S signed
#define U unsigned
#define L long long
using Byte= U char;
using U8= uint8_t;
using U16= uint16_t;
using U32= uint32_t;
using U64= uint64_t;
using S8= int8_t;
using S16= int16_t;
using S32= int32_t;
using S64= int64_t;
using Size= std::size_t;
#define tp template
#define tn typename
#define ns namespace
#define nl "\n"
#define tab "\t"
#define op operator
#define WATCH(x) out(#x ": ",x,nl)
using Size= std::size_t;
// let's define an order for declaration specifiers
// typedef, static, <the base type>, mutable, const, volatile
// let's not use volatile ever
tp<tn... A> void out(A const&... a) {
	(void)(S[]){(std::cout << a,0)...};
}
tp<tn... A> void error(A const&... a) {
	out("error: ",a...,nl);
	std::exit(1);
}
// https://stackoverflow.com/questions/281818/unmangling-the-result-of-stdtype-infoname
// useful for printing a generated type at runtime
tp<tn A> std::string typeName() {
	char const* name= typeid(A).name();
	S status;
	std::unique_ptr<char,void(*)(void*)> res {
		abi::__cxa_demangle(name,NULL,NULL,&status),
		std::free
	};
	return (status==0) ? res.get() : name;
}
