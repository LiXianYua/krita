#include "PkDataStream.h"
#include "PkVariant.h"
#include <cstdio>
#include <algorithm>
#include <type_traits>
#include <vector>
class QDataStream;
static_assert(!std::is_same<PkDataStream,QDataStream>::value);
namespace {
void hex(const PkByteArray&b){for(int i=0;i<b.size();++i)std::printf("%02x",(unsigned char)b.constData()[i]);}
const char*vname(PkDataStream::Version v){return v==PkDataStream::Qt_4_6?"qt46":"qt515";} const char*oname(PkDataStream::ByteOrder o){return o==PkDataStream::BigEndian?"big":"little";}
PkByteArray hx(const char*h){std::vector<unsigned char>b;for(;*h;h+=2){auto n=[](char c){return(unsigned char)(c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c-'A'+10);};b.push_back((n(h[0])<<4)|n(h[1]));}return PkByteArray(b);}
PkByteArray add(PkByteArray a,const PkByteArray&b){std::vector<unsigned char>v;for(int i=0;i<a.size();++i)v.push_back((unsigned char)a.constData()[i]);for(int i=0;i<b.size();++i)v.push_back((unsigned char)b.constData()[i]);return PkByteArray(v);}
void pre(const char*k,PkDataStream::Version v,PkDataStream::ByteOrder o,const char*n,const PkByteArray&i){std::printf("kind=%s version=%s order=%s case=%s input=",k,vname(v),oname(o),n);hex(i);}
void bare(PkDataStream::Version v,PkDataStream::ByteOrder o,const char*n,const PkByteArray&i){bool x=true;PkDataStream s(i);s.setVersion(v);s.setByteOrder(o);s>>x;pre("bare",v,o,n,i);std::printf(" value=%u status=%d\n",x,int(s.status()));}
void var(PkDataStream::Version v,PkDataStream::ByteOrder o,const char*n,const PkByteArray&i){PkVariant x(true);PkDataStream s(i);s.setVersion(v);s.setByteOrder(o);s>>x;pre("variant",v,o,n,i);std::printf(" valid=%u type=%d bool=%u status=%d\n",x.isValid(),x.type(),x.toBool(),int(s.status()));}
void rect(PkDataStream::Version v,PkDataStream::ByteOrder o,const char*n,const PkByteArray&i){PkVariant x;PkDataStream s(i);s.setVersion(v);s.setByteOrder(o);s>>x;PkRect r=x.toRect();pre("rect",v,o,n,i);std::printf(" valid=%u x=%d y=%d w=%d h=%d status=%d\n",x.isValid(),r.x(),r.y(),r.width(),r.height(),int(s.status()));}
PkByteArray frame(const char*f,const char*p,PkDataStream::ByteOrder o){PkByteArray x=hx(o==PkDataStream::BigEndian?"0000000100":"0100000000");x.data()[4]=hx(f).constData()[0];return add(x,hx(p));}
PkByteArray rectFrame(const char*p,PkDataStream::ByteOrder o){PkByteArray c=hx(p);if(o==PkDataStream::LittleEndian)for(int i=0;i<c.size();i+=4)std::swap(c.data()[i],c.data()[i+3]),std::swap(c.data()[i+1],c.data()[i+2]);return add(hx(o==PkDataStream::BigEndian?"0000001300":"1300000000"),c);}
}
int main(){const char*b[]={"00","01","02","ff"};for(auto v:{PkDataStream::Qt_4_6,PkDataStream::Qt_5_15})for(auto o:{PkDataStream::BigEndian,PkDataStream::LittleEndian}){for(auto q:b)bare(v,o,q,hx(q));for(auto q:b)var(v,o,q,frame(q,"01",o));for(auto q:b)var(v,o,q,frame("00",q,o));rect(v,o,"int-min-max",rectFrame("80000000800000007fffffff7fffffff",o));rect(v,o,"reversed-extremes",rectFrame("7fffffff7fffffff8000000080000000",o));}}
