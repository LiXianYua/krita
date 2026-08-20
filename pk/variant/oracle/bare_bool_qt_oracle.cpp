#include <QByteArray>
#include <QDataStream>
#include <QRect>
#include <QVariant>
#include <cstdio>
#include <algorithm>
#include <type_traits>
class PkDataStream;
static_assert(!std::is_same<QDataStream, PkDataStream>::value);
namespace {
void hex(const QByteArray &b){for(unsigned char x:b)std::printf("%02x",x);}
const char*vname(QDataStream::Version v){return v==QDataStream::Qt_4_6?"qt46":"qt515";}
const char*oname(QDataStream::ByteOrder o){return o==QDataStream::BigEndian?"big":"little";}
void pre(const char*k,QDataStream::Version v,QDataStream::ByteOrder o,const char*n,const QByteArray&i){std::printf("kind=%s version=%s order=%s case=%s input=",k,vname(v),oname(o),n);hex(i);}
void bare(QDataStream::Version v,QDataStream::ByteOrder o,const char*n,const QByteArray&i){bool x=true;QDataStream s(i);s.setVersion(v);s.setByteOrder(o);s>>x;pre("bare",v,o,n,i);std::printf(" value=%u status=%d\n",x,int(s.status()));}
void var(QDataStream::Version v,QDataStream::ByteOrder o,const char*n,const QByteArray&i){quint32 type=0;quint8 flag=0;quint8 b=0;QDataStream s(i);s.setVersion(v);s.setByteOrder(o);s>>type>>flag>>b;pre("variant",v,o,n,i);std::printf(" valid=%u type=%u bool=%u status=%d\n",s.status()==QDataStream::Ok,type,b!=0,int(s.status()));}
void rect(QDataStream::Version v,QDataStream::ByteOrder o,const char*n,const QByteArray&i){QVariant x;QDataStream s(i);s.setVersion(v);s.setByteOrder(o);s>>x;QRect r=x.toRect();pre("rect",v,o,n,i);std::printf(" valid=%u x=%d y=%d w=%d h=%d status=%d\n",x.isValid(),r.x(),r.y(),r.width(),r.height(),int(s.status()));}
QByteArray frame(const char*f,const char*p,QDataStream::ByteOrder o){QByteArray x=QByteArray::fromHex(o==QDataStream::BigEndian?"0000000100":"0100000000");x[4]=QByteArray::fromHex(f)[0];x+=QByteArray::fromHex(p);return x;}
QByteArray rectFrame(const char*p,QDataStream::ByteOrder o){QByteArray x=QByteArray::fromHex(o==QDataStream::BigEndian?"0000001300":"1300000000");QByteArray c=QByteArray::fromHex(p);if(o==QDataStream::LittleEndian)for(int i=0;i<c.size();i+=4){char t=c[i];c[i]=c[i+3];c[i+3]=t;t=c[i+1];c[i+1]=c[i+2];c[i+2]=t;}x+=c;return x;}
}
int main(){const char*b[]={"00","01","02","ff"};for(auto v:{QDataStream::Qt_4_6,QDataStream::Qt_5_15})for(auto o:{QDataStream::BigEndian,QDataStream::LittleEndian}){for(auto q:b)bare(v,o,q,QByteArray::fromHex(q));for(auto q:b)var(v,o,q,frame(q,"01",o));for(auto q:b)var(v,o,q,frame("00",q,o));rect(v,o,"int-min-max",rectFrame("80000000800000007fffffff7fffffff",o));rect(v,o,"reversed-extremes",rectFrame("7fffffff7fffffff8000000080000000",o));}}
