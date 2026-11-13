struct FreeBlock {
    offset: u64, size: u64
}

fn remove_block(blocks: FreeBlock[:], count: i64, index: i64) -> i64 {
    assert(index >= 0 && index < count && count <= blocks.len);
    var i = index;

    while i < count - 1 {
        blocks[i] = blocks[i + 1];
        i += 1;
    }
    return count - 1;
}

fn goose_release_block(blocks: FreeBlock[:], count: i64, offset: u64, size: u64) -> i64 {
    assert(count >= 0 && count <= blocks.len);
    for i in count {
        let current = & blocks[i];

        if current.offset + current.size == offset {
            current.size += size;

            if i < count - 1 {
                let successor = & blocks[i + 1];

                if current.offset + current.size == successor.offset {
                    current.size += successor.size;
                    return remove_block(blocks, count, i + 1);
                }
            }
            return count;
        }

        if offset + size == current.offset {
            current.offset = offset;
            current.size += size;

            if i > 0 {
                let predecessor = & blocks[i - 1];

                if predecessor.offset + predecessor.size == current.offset {
                    predecessor.size += current.size;
                    return remove_block(blocks, count, i);
                }
            }
            return count;
        }
    }
    assert(count < blocks.len);
    var insert: i64 = 0;

    while insert < count && blocks[insert].offset < offset {
        insert += 1;
    }
    var j = count;

    while j > insert {
        blocks[j] = blocks[j - 1];
        j -= 1;
    }
    blocks[insert] = FreeBlock {
        offset: offset, size: size
    }
    ;
    return count + 1;
}

fn main() {
    var blocks: FreeBlock[4] = default < FreeBlock[4] > ();
    blocks[0] = FreeBlock {
        offset: 0, size: 16
    }
    ;
    blocks[1] = FreeBlock {
        offset: 32, size: 16
    }
    ;
    let count = goose_release_block(blocks, 2, 16, 16);
    assert(count == 1 && blocks[0].offset == 0 && blocks[0].size == 48);
}

