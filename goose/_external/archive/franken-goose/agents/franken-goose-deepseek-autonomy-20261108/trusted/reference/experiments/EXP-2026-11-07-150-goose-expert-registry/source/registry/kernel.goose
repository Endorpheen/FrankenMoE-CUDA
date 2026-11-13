extern "fg150_registry_meta" fn meta(c:i64,field:i64)->i64;
extern "fg150_registry_action" fn action(c:i64,op:i64,a:i64,b:i64)->i64;
extern "fg150_registry_witness" fn witness(mode:i64);
fn space(ch:i64)->bool {return ch==32||(ch>=9&&ch<=13);}
fn brand(c:i64)->i64 {
    let platform=meta(c,6);
    if platform==1 {
        let size=action(c,18,0,0);if size<=0 {return 0;}
        let resized=action(c,19,size,0);let fetched=action(c,20,0,0);return 1;
    }
    if platform==2 {
        let handle=action(c,21,0,0);if handle==0 {return 0;}
        let size=action(c,22,handle,0);var result:i64=0;
        if size>0 {
            let resized=action(c,19,size,0);
            if action(c,23,handle,0)!=0 {
                var end:i64=0;while end<size&&meta(c,10000+end)!=0 {end+=1;}
                if end<size {let trimmed=action(c,19,end,0);}result=1;
            }
        }
        let closed=action(c,24,handle,0);return result;
    }
    if platform!=0 {return 0;}
    let handle=action(c,0,0,0);if handle==0 {return 0;}
    var found:i64=0;
    while action(c,1,handle,0)!=0 {
        let length=meta(c,0);var prefix=length>=10;
        let expected:i64[10]=[109,111,100,101,108,32,110,97,109,101];
        for i in 10 {if i>=length||meta(c,100+i)!=expected[i] {prefix=false;}}
        if prefix {
            var colon:i64=0;while colon<length&&meta(c,100+colon)!=58 {colon+=1;}
            if colon<length {
                var begin=colon+1;var end=length;
                while begin<end&&space(meta(c,100+begin)) {begin+=1;}
                while end>begin&&space(meta(c,100+end-1)) {end-=1;}
                let ignored=action(c,2,begin,end);found=1;break;
            }
        }
    }
    let ignored=action(c,3,handle,0);return found;
}
fn entry(mode:i64,c:i64)->i64 {
    witness(mode);
    if mode==0 {return brand(c);}
    if mode==1 {assert(meta(c,1)==0);return action(c,4,0,0);}
    if mode==2 {let ignored=action(c,5,0,0);return action(c,6,0,0);}
    if mode==3 {
        let a=action(c,7,0,0);let b=action(c,7,1,0);let d=action(c,7,2,0);let e=action(c,7,3,0);
        for i in 5 {let enabled=if i==2||i==4 {1}else {0};let ignored=action(c,8,i,enabled);}return 0;
    }
    if mode==4 {return action(c,9,0,0);}
    if mode==5 {return meta(c,2);}
    if mode==6 {return action(c,10,0,0);}
    if mode==7 {return action(c,11,0,0);}
    if mode==8 {assert(meta(c,3)>=0);return action(c,12,0,0);}
    if mode==9 {return action(c,13,0,0);}
    if mode==10 {
        let size=meta(c,3);let align=meta(c,4);assert(size>=0&&align>0&&(align&(align-1))==0);
        let ptr=action(c,14,align,size);if ptr==0 {return 0;}
        let buffer=action(c,15,ptr,size);if buffer==0 {let ignored=action(c,16,ptr,0);return 0;}
        let configured=action(c,17,buffer,0);return buffer;
    }
    assert(mode==11);let ignored=action(c,16,meta(c,5),0);return 0;
}
fn main(){print(entry(4,0));}
