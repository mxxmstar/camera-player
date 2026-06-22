#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

// ---------------------------------------------------------------------------
// BitWriter -- append bits, outputs aligned bytes
// ---------------------------------------------------------------------------
class BitWriter {
public:
    std::vector<uint8_t> bytes;
    uint8_t buf;
    int bits_in_buf;

    BitWriter() : buf(0), bits_in_buf(0) {}

    void write_bit(int bit) {
        buf = (buf << 1) | (bit & 1);
        if (++bits_in_buf == 8) {
            bytes.push_back(buf);
            buf = 0;
            bits_in_buf = 0;
        }
    }

    void write_bits(int value, int n) {
        for (int i = n - 1; i >= 0; --i)
            write_bit((value >> i) & 1);
    }

    // unsigned Exp-Golomb (ue(v))
    void write_ue(unsigned int value) {
        unsigned int v = value + 1;
        int leading_zeros = 0;
        unsigned int tmp = v;
        while (tmp >>= 1)
            ++leading_zeros;
        for (int i = 0; i < leading_zeros; ++i)
            write_bit(0);
        write_bits(v, leading_zeros + 1);
    }

    // signed Exp-Golomb (se(v))
    void write_se(int value) {
        unsigned int code_num;
        if (value <= 0)
            code_num = (unsigned int)(-value) * 2;
        else
            code_num = (unsigned int)(value) * 2 - 1;
        write_ue(code_num);
    }

    // flush: write rbsp_stop_one_bit then zero-pad to byte boundary
    void flush() {
        write_bit(1);
        while (bits_in_buf != 0)
            write_bit(0);
    }

    void clear() {
        bytes.clear();
        buf = 0;
        bits_in_buf = 0;
    }
};

// ---------------------------------------------------------------------------
// Emulation prevention: insert 0x03 before forbidden byte sequences
// ---------------------------------------------------------------------------
static void write_nal(FILE* f, uint8_t nal_header, BitWriter& bw) {
    // start code
    const uint8_t start_code[4] = { 0x00, 0x00, 0x00, 0x01 };
    fwrite(start_code, 1, 4, f);

    // NAL header byte with emulation prevention
    int count = 0;
    if (count == 2 && (nal_header & 0xFC) == 0x00) {
        fputc(0x03, f);
        count = 0;
    }
    fputc(nal_header, f);
    if (nal_header == 0x00)
        ++count;
    else
        count = 0;

    // payload bytes with emulation prevention
    const auto& payload = bw.bytes;
    for (size_t i = 0; i < payload.size(); ++i) {
        uint8_t b = payload[i];
        if (count == 2 && (b & 0xFC) == 0x00) {
            fputc(0x03, f);
            count = 0;
        }
        fputc(b, f);
        if (b == 0x00)
            ++count;
        else
            count = 0;
    }
}

// ---------------------------------------------------------------------------
// Build SPS RBSP for Baseline, level 1.1, QCIF 176x144
// ---------------------------------------------------------------------------
static void build_sps(BitWriter& bw) {
    bw.clear();
    bw.write_bits(66, 8);      // profile_idc = Baseline
    bw.write_bits(0x80, 8);    // constraint_set0_flag=1, others 0, reserved=00
    bw.write_bits(11, 8);      // level_idc = 11 (level 1.1)

    bw.write_ue(0);            // seq_parameter_set_id
    bw.write_ue(0);            // log2_max_frame_num_minus4
    bw.write_ue(0);            // pic_order_cnt_type
    bw.write_ue(0);            // log2_max_pic_order_cnt_lsb_minus4
    bw.write_ue(1);            // max_num_ref_frames
    bw.write_bit(0);           // gaps_in_frame_num_value_allowed_flag
    bw.write_ue(10);           // pic_width_in_mbs_minus1 (11 MBs wide)
    bw.write_ue(8);            // pic_height_in_map_units_minus1 (9 MBs tall)
    bw.write_bit(1);           // frame_mbs_only_flag
    bw.write_bit(1);           // direct_8x8_inference_flag
    bw.write_bit(0);           // frame_cropping_flag
    bw.write_bit(0);           // vui_parameters_present_flag
    bw.flush();
}

// ---------------------------------------------------------------------------
// Build PPS RBSP
// ---------------------------------------------------------------------------
static void build_pps(BitWriter& bw) {
    bw.clear();
    bw.write_ue(0);            // pic_parameter_set_id
    bw.write_ue(0);            // seq_parameter_set_id
    bw.write_bit(0);           // entropy_coding_mode_flag (CAVLC)
    bw.write_bit(0);           // bottom_field_pic_order_present_flag
    bw.write_ue(0);            // num_slice_groups_minus1
    bw.write_ue(0);            // num_ref_idx_l0_active_minus1
    bw.write_ue(0);            // num_ref_idx_l1_active_minus1
    bw.write_bit(0);           // weighted_pred_flag
    bw.write_bits(0, 2);       // weighted_bipred_idc
    bw.write_se(0);            // pic_init_qp_minus26
    bw.write_se(0);            // pic_init_qs_minus26
    bw.write_se(0);            // chroma_qp_index_offset
    bw.write_bit(0);           // deblocking_filter_control_present_flag
    bw.write_bit(0);           // constrained_intra_pred_flag
    bw.write_bit(0);           // redundant_pic_cnt_present_flag
    bw.flush();
}

// ---------------------------------------------------------------------------
// Build one IDR slice (I slice with zero residuals)
// ---------------------------------------------------------------------------
static void build_idr_slice(BitWriter& bw, unsigned frame_num) {
    bw.clear();

    // ----- slice header (I slice, IDR) -----
    bw.write_ue(0);            // first_mb_in_slice
    bw.write_ue(2);            // slice_type = I slice
    bw.write_ue(0);            // pic_parameter_set_id
    bw.write_bits(frame_num, 1); // frame_num (1 bit, 0 for IDR)
    bw.write_ue(0);            // idr_pic_id
    bw.write_bits(0, 1);       // pic_order_cnt_lsb (1 bit)
    // dec_ref_pic_marking (IDR)
    bw.write_bit(1);           // no_output_of_prior_pics_flag
    bw.write_bit(0);           // long_term_reference_flag

    // ----- macroblock layer (99 MBs, zero residuals) -----
    // width=11, height=9 => 99 macroblocks
    for (int i = 0; i < 99; ++i) {
        // mb_type = 1 (I_16x16_0_0_0: vertical pred, chroma DC pred 0, CBP=0)
        bw.write_ue(1);

        // intra_chroma_pred_mode = 0
        bw.write_ue(0);

        // Residual (I_16x16, CBP=0):
        //   - luma DC block   (16 coeffs, all zero) -> coeff_token (0,0)
        //   - chroma U DC blk (4 coeffs, all zero)  -> coeff_token (0,0)
        //   - chroma V DC blk (4 coeffs, all zero)  -> coeff_token (0,0)
        // (TotalCoeff=0, so no trailing_ones, no levels, no total_zeros, no run_before)
        bw.write_bit(1);
        bw.write_bit(1);
        bw.write_bit(1);
    }

    // rbsp_slice_trailing_bits
    bw.flush();
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
    FILE* f = fopen("E:\\test.h264", "wb");
    if (!f) {
        fprintf(stderr, "Failed to open output file\n");
        return 1;
    }

    BitWriter bw;

    // --- SPS (NAL type 7) ---
    build_sps(bw);
    write_nal(f, 0x67, bw);

    // --- PPS (NAL type 8) ---
    build_pps(bw);
    write_nal(f, 0x68, bw);

    // --- 12 IDR slices (NAL type 5) ---
    for (int frame = 0; frame < 12; ++frame) {
        build_idr_slice(bw, 0);
        write_nal(f, 0x65, bw);
    }

    fclose(f);
    return 0;
}
