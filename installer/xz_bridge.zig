const std = @import("std");

export fn dlb_xz_decompress(
    src_ptr: [*]const u8,
    src_len: usize,
    dst_ptr: [*]u8,
    dst_cap: usize,
) callconv(.c) isize {
    var in_reader = std.Io.Reader.fixed(src_ptr[0..src_len]);
    const gpa = std.heap.page_allocator;

    var dec = std.compress.xz.Decompress.init(&in_reader, gpa, &.{}) catch return -1;
    defer dec.deinit();

    var total: usize = 0;
    while (total < dst_cap) {
        const n = dec.reader.readSliceShort(dst_ptr[total..dst_cap]) catch return -2;
        if (n == 0) break;
        total += n;
    }
    return @intCast(total);
}
