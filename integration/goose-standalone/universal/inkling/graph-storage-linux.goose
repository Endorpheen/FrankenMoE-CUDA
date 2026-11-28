extern "aligned_alloc" fn graph_storage_aligned(alignment: u64,size: u64) -> c_ptr<void>;
extern fn graph_storage_fopen(path: u64,mode: u64) -> u64;
extern fn graph_storage_fgets(line: u64,size: i32,file: u64) -> u64;
extern fn graph_storage_fclose(file: u64) -> i32;

export fn fg143_storage_alloc(bytes: u64) -> u64 {
    let pointer=graph_storage_aligned(16,bytes);
    var address: u64=0;
    graph_host_copy(unsafe_c_address<c_ptr<void>>(address),unsafe_c_address<c_ptr<const void>>(pointer),8);
    return address;
}

fn graph_storage_parse(line: const u8[:],available: u64&) -> bool {
    let key: const u8[:]="MemAvailable:";
    if line.len<=key.len || line[0..key.len]!=key { return false; }
    var at=key.len;
    while at<line.len && (line[at]==32 || (line[at]>=9 && line[at]<=13)) { at+=1; }
    let negative=at<line.len && line[at]==45;
    if at<line.len && (negative || line[at]==43) { at+=1; }
    let start=at;
    var kib: u64=0;
    while at<line.len && line[at]>=48 && line[at]<=57 {
        let digit=(line[at]-48) as! u64;
        if kib>(0xffffffffffffffff-digit)/10 { available=0; return true; }
        kib=kib*10+digit;
        at+=1;
    }
    if at==start { return false; }
    if negative { kib=0-kib; }
    available=if kib<=0x3fffffffffffff { kib*1024 } else { 0 };
    return true;
}

export fn fg143_storage_ram_available() -> u64 {
    var path: u8[14];
    let filename: const u8[:]="/proc/meminfo";
    for i in filename.len { path[i]=filename[i]; }
    path[filename.len]=0;
    let mode: u8[2]=[114,0];
    let file=graph_storage_fopen(graph_host_address(unsafe_c_address<c_ptr<const void>>(path[0])),
        graph_host_address(unsafe_c_address<c_ptr<const void>>(mode[0])));
    if file==0 { return 0; }
    var line: u8[128];
    let address=graph_host_address(unsafe_c_address<c_ptr<const void>>(line[0]));
    var available: u64=0;
    while graph_storage_fgets(address,128,file)!=0 {
        if graph_storage_parse(line,available) { break; }
    }
    graph_storage_fclose(file);
    return available;
}
