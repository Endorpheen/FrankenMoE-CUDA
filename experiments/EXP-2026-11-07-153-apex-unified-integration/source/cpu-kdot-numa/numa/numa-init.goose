// numa-init.goose - Goose port of the remaining native affinity-control loop.
//
// Source: inputs/native/ggml/src/ggml-cpu/ggml-cpu.c
//         ggml_numa_init(), lines 637-723.
//
// Already Goose elsewhere (not duplicated here):
//   - ggml_barrier / chunk set/add -> pool kernel
//   - worker lifecycle/pause/wakeup/kickoff -> pool kernel
//   - per-worker graph job dispatch -> executor kernel
//   - set/clear_numa_thread_affinity placement -> pool kernel numa()
//
// Still native and owned here: the one-time NUMA enumeration and validity
// decision that produces g_state.numa:
//   * early return when n_nodes is already non-zero,
//   * publish strategy and the current thread affinity snapshot,
//   * probe node0..node7 and cpu0..cpu511 existence,
//   * ask the kernel for the current cpu/node,
//   * invalidate when no node/cpu was found or getcpu failed,
//   * per node, probe nodeN/cpuC membership,
//   * warn when more than one node has numa_balancing enabled.
//
// Services provide only O(1) OS primitives (stat probe, pthread affinity
// snapshot, getcpu syscall, diagnostics).  All loops and decisions are Goose.

extern "fg153_np_set_strategy" fn set_strategy(result:i64,strategy:i64)->i64;
extern "fg153_np_snapshot_cpuset" fn snapshot_cpuset(result:i64)->i64;
extern "fg153_np_node_exists" fn node_exists(node:i64)->i64;
extern "fg153_np_cpu_exists" fn cpu_exists(cpu:i64)->i64;
extern "fg153_np_node_cpu_exists" fn node_cpu_exists(node:i64,cpu:i64)->i64;
extern "fg153_np_getcpu" fn getcpu(result:i64)->i64;
extern "fg153_np_set_counts" fn set_counts(result:i64,nodes:i64,cpus:i64)->i64;
extern "fg153_np_set_node_ncpus" fn set_node_ncpus(result:i64,node:i64,count:i64)->i64;
extern "fg153_np_set_node_cpu" fn set_node_cpu(result:i64,node:i64,index:i64,cpu:i64)->i64;
extern "fg153_np_balancing" fn balancing()->i64;
extern "fg153_np_warn" fn warn(code:i64);
extern "fg153_np_witness" fn witness(mode:i64);

fn ensure(ok:bool) {if !ok {witness(7);assert(false);}}

fn run(result:i64,strategy:i64,existing:i64)->i64 {
    witness(0);
    if existing>0 {warn(0);return 0;}
    ensure(result!=0);
    ensure(strategy>=0&&strategy<=4);
    let ignored=set_strategy(result,strategy);
    let ignored2=snapshot_cpuset(result);

    var nodes:i64=0;
    while nodes<8 {
        if node_exists(nodes)==0 {break;}
        nodes+=1;
    }
    var cpus:i64=0;
    while cpus<512 {
        if cpu_exists(cpus)==0 {break;}
        cpus+=1;
    }
    let current=getcpu(result);
    let ignored3=set_counts(result,nodes,cpus);
    if nodes<1||cpus<1||current<0 {
        let ignored4=set_counts(result,0,cpus);
        witness(1);
        return 2;
    }

    var n:i64=0;
    while n<nodes {
        var count:i64=0;
        var c:i64=0;
        while c<cpus {
            if node_cpu_exists(n,c)!=0 {
                let ignored5=set_node_cpu(result,n,count,c);
                count+=1;
            }
            c+=1;
        }
        let ignored6=set_node_ncpus(result,n,count);
        n+=1;
    }

    if nodes>1&&balancing()!=0 {warn(2);}
    witness(2);
    return 1;
}

fn entry(result:i64,strategy:i64,existing:i64)->i64 {
    if result==0 {witness(6);return 0;}
    return run(result,strategy,existing);
}

fn main() {print(entry(0,0,0));}
