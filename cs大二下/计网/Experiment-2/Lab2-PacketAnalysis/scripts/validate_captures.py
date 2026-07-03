from __future__ import annotations

import csv
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
EXPORTS_DIR = PROJECT_ROOT / "exports"
CAPTURES_DIR = PROJECT_ROOT / "captures"
SUMMARY_PATH = EXPORTS_DIR / "analysis_summary.md"


@dataclass
class Result:
    status: str
    detail: str


def read_csv(name: str) -> list[dict[str, str]]:
    path = EXPORTS_DIR / name
    if not path.is_file():
        return []
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        return list(csv.DictReader(handle))


def value(row: dict[str, str], *names: str) -> str:
    for name in names:
        if name in row and row[name] != "":
            return row[name]
    return ""


def as_int(text: str) -> int | None:
    if not text:
        return None
    normalized = text.strip().lower()
    if normalized == "true":
        return 1
    if normalized == "false":
        return 0
    try:
        return int(text, 0)
    except ValueError:
        return None


def validate_source(
    rows: list[dict[str, str]], csv_name: str, capture_name: str
) -> Result | None:
    if not rows:
        return Result("FAIL", f"{csv_name} missing or empty")
    if not (CAPTURES_DIR / capture_name).is_file():
        return Result("FAIL", f"{capture_name} missing; CSV provenance cannot be verified")

    frames = [value(row, "frame.number") for row in rows]
    parsed_frames = [as_int(frame) for frame in frames]
    if any(frame is None or frame <= 0 for frame in parsed_frames):
        return Result("FAIL", "Missing or invalid Frame Number")
    if len(frames) != len(set(frames)):
        return Result("FAIL", "Duplicate Frame Number prevents unique packet lookup")
    return None


def validate_dhcp(rows: list[dict[str, str]]) -> tuple[Result, list[str]]:
    source_error = validate_source(rows, "dhcp_fields.csv", "dhcp.pcapng")
    if source_error:
        return source_error, []

    message_names = {
        "1": "Discover",
        "2": "Offer",
        "3": "Request",
        "4": "Decline",
        "5": "ACK",
        "6": "NAK",
        "7": "Release",
        "8": "Inform",
    }
    transactions: dict[str, set[str]] = defaultdict(set)
    releases: list[str] = []
    table: list[str] = []
    for row in rows:
        raw_type = value(row, "dhcp.option.dhcp", "bootp.option.dhcp")
        name = message_names.get(raw_type, raw_type or "Unknown")
        frame = value(row, "frame.number")
        transaction = value(row, "dhcp.id", "bootp.id")
        if transaction:
            transactions[transaction].add(name.lower())
        if name.lower() == "release":
            releases.append(frame)
        table.append(
            f"| {name} | {frame} | {value(row, 'ip.src')} | "
            f"{value(row, 'ip.dst')} | {transaction} |"
        )

    required = {"discover", "offer", "request", "ack"}
    complete = [
        transaction
        for transaction, message_types in transactions.items()
        if required <= message_types
    ]
    if not complete:
        return Result("FAIL", "No single Transaction ID contains complete DORA"), table
    release_note = (
        f"Release present in frame(s) {', '.join(releases)}"
        if releases
        else "Release not captured"
    )
    return Result(
        "PASS",
        f"Complete DORA in {len(complete)} transaction(s); {release_note}",
    ), table


def validate_icmp(rows: list[dict[str, str]]) -> tuple[Result, list[str]]:
    source_error = validate_source(rows, "icmp_fields.csv", "icmp.pcapng")
    if source_error:
        return source_error, []

    requests: dict[tuple[str, str, str, str], list[str]] = defaultdict(list)
    pairs: list[str] = []
    for row in rows:
        icmp_type = value(row, "icmp.type")
        ident = value(row, "icmp.ident")
        sequence = value(row, "icmp.seq")
        src = value(row, "ip.src")
        dst = value(row, "ip.dst")
        frame = value(row, "frame.number")
        if icmp_type == "8":
            requests[(ident, sequence, src, dst)].append(frame)
        elif icmp_type == "0":
            request_frames = requests.get((ident, sequence, dst, src))
            if request_frames:
                request_frame = request_frames.pop(0)
                pairs.append(
                    f"| {len(pairs) + 1} | {request_frame} | {frame} | "
                    f"{ident} | {sequence} |"
                )

    if not pairs:
        return Result("FAIL", "No matched Echo Request/Reply pair"), []
    return Result("PASS", f"{len(pairs)} matched Echo pair(s)"), pairs


def validate_fragments(rows: list[dict[str, str]]) -> tuple[Result, list[str]]:
    source_error = validate_source(
        rows, "ip_fragment_fields.csv", "ip_fragment.pcapng"
    )
    if source_error:
        return source_error, []

    groups: dict[tuple[str, str, str, str], list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        key = (
            value(row, "ip.src"),
            value(row, "ip.dst"),
            value(row, "ip.proto"),
            value(row, "ip.id"),
        )
        groups[key].append(row)

    table: list[str] = []
    complete_count = 0
    for key, group in groups.items():
        fragments = []
        for row in group:
            offset_units = as_int(value(row, "ip.frag_offset"))
            total_length = as_int(value(row, "ip.len"))
            header_length = as_int(value(row, "ip.hdr_len"))
            mf = as_int(value(row, "ip.flags.mf"))
            if None in (offset_units, total_length, header_length, mf):
                continue
            payload_length = total_length - header_length
            fragments.append(
                (
                    offset_units * 8,
                    offset_units * 8 + payload_length,
                    mf,
                    value(row, "frame.number"),
                )
            )

        fragments.sort()
        has_more = any(fragment[2] == 1 for fragment in fragments)
        has_last = any(fragment[2] == 0 and fragment[0] > 0 for fragment in fragments)
        continuous = bool(fragments) and fragments[0][0] == 0
        for previous, current in zip(fragments, fragments[1:]):
            if previous[1] != current[0]:
                continuous = False
                break
        complete = len(fragments) >= 2 and has_more and has_last and continuous
        if complete:
            complete_count += 1
        frames = ", ".join(fragment[3] for fragment in fragments)
        table.append(
            f"| {key[3]} | {len(fragments)} | {frames} | "
            f"{'PASS' if complete else 'FAIL'} |"
        )

    if complete_count == 0:
        return Result("FAIL", "No complete, continuous IPv4 fragment group"), table
    return Result("PASS", f"{complete_count} complete fragment group(s)"), table


def validate_tcp(rows: list[dict[str, str]]) -> tuple[Result, list[str]]:
    source_error = validate_source(rows, "tcp_fields.csv", "tcp.pcapng")
    if source_error:
        return source_error, []

    streams: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        streams[value(row, "tcp.stream")].append(row)

    table: list[str] = []
    complete_streams = 0
    reset_streams = 0
    for stream, packets in streams.items():
        packets.sort(key=lambda row: as_int(value(row, "frame.number")) or 0)
        initial_syn = next(
            (
                row
                for row in packets
                if as_int(value(row, "tcp.flags.syn")) == 1
                and as_int(value(row, "tcp.flags.ack")) == 0
            ),
            None,
        )
        syn_ack = None
        final_ack = None
        if initial_syn:
            client_ip = value(initial_syn, "ip.src")
            client_port = value(initial_syn, "tcp.srcport")
            server_ip = value(initial_syn, "ip.dst")
            server_port = value(initial_syn, "tcp.dstport")
            syn_frame = as_int(value(initial_syn, "frame.number")) or 0
            syn_ack = next(
                (
                    row
                    for row in packets
                    if (as_int(value(row, "frame.number")) or 0) > syn_frame
                    and value(row, "ip.src") == server_ip
                    and value(row, "tcp.srcport") == server_port
                    and value(row, "ip.dst") == client_ip
                    and value(row, "tcp.dstport") == client_port
                    and as_int(value(row, "tcp.flags.syn")) == 1
                    and as_int(value(row, "tcp.flags.ack")) == 1
                ),
                None,
            )
            if syn_ack:
                syn_ack_frame = as_int(value(syn_ack, "frame.number")) or 0
                final_ack = next(
                    (
                        row
                        for row in packets
                        if (as_int(value(row, "frame.number")) or 0) > syn_ack_frame
                        and value(row, "ip.src") == client_ip
                        and value(row, "tcp.srcport") == client_port
                        and value(row, "ip.dst") == server_ip
                        and value(row, "tcp.dstport") == server_port
                        and as_int(value(row, "tcp.flags.syn")) == 0
                        and as_int(value(row, "tcp.flags.ack")) == 1
                    ),
                    None,
                )

        handshake = initial_syn is not None and syn_ack is not None and final_ack is not None
        fin_directions = {
            (
                value(row, "ip.src"),
                value(row, "tcp.srcport"),
                value(row, "ip.dst"),
                value(row, "tcp.dstport"),
            )
            for row in packets
            if as_int(value(row, "tcp.flags.fin")) == 1
        }
        reset = any(as_int(value(row, "tcp.flags.reset")) == 1 for row in packets)
        normal_close = len(fin_directions) >= 2 and not reset
        if handshake and normal_close:
            complete_streams += 1
        elif handshake and reset:
            reset_streams += 1

        first = initial_syn or packets[0]
        client = f"{value(first, 'ip.src')}:{value(first, 'tcp.srcport')}"
        server = f"{value(first, 'ip.dst')}:{value(first, 'tcp.dstport')}"
        table.append(
            f"| {stream} | {client} | {server} | "
            f"{'PASS' if handshake else 'FAIL'} | "
            f"{'PASS' if normal_close else ('RST' if reset else 'FAIL')} |"
        )

    if complete_streams == 0:
        if reset_streams:
            return Result(
                "WARN",
                f"{reset_streams} stream(s) completed handshake but ended with RST",
            ), table
        return Result("FAIL", "No stream has both handshake and normal FIN close"), table
    return Result("PASS", f"{complete_streams} complete TCP stream(s)"), table


def capture_status(name: str) -> str:
    return "PASS" if (CAPTURES_DIR / name).is_file() else "FAIL"


def main() -> int:
    dhcp_result, dhcp_table = validate_dhcp(read_csv("dhcp_fields.csv"))
    icmp_result, icmp_table = validate_icmp(read_csv("icmp_fields.csv"))
    fragment_result, fragment_table = validate_fragments(
        read_csv("ip_fragment_fields.csv")
    )
    tcp_result, tcp_table = validate_tcp(read_csv("tcp_fields.csv"))

    results = {
        "DHCP": dhcp_result,
        "ICMP": icmp_result,
        "IPv4 fragments": fragment_result,
        "TCP": tcp_result,
    }

    lines = [
        "# 实验二抓包分析汇总",
        "",
        "## 1. 文件检查",
        "",
        "| 文件 | 状态 |",
        "|---|---|",
        f"| captures/dhcp.pcapng | {capture_status('dhcp.pcapng')} |",
        f"| captures/icmp.pcapng | {capture_status('icmp.pcapng')} |",
        f"| captures/ip_fragment.pcapng | {capture_status('ip_fragment.pcapng')} |",
        f"| captures/tcp.pcapng | {capture_status('tcp.pcapng')} |",
        "",
        "## 2. DHCP 分析摘要",
        "",
        "| Message Type | Frame | Src IP | Dst IP | Transaction ID |",
        "|---|---:|---|---|---|",
        *(dhcp_table or ["| 缺失 |  |  |  |  |"]),
        "",
        "## 3. ICMP 分析摘要",
        "",
        "| Pair | Request Frame | Reply Frame | Identifier | Sequence |",
        "|---:|---:|---:|---|---|",
        *(icmp_table or ["| 缺失 |  |  |  |  |"]),
        "",
        "## 4. IP 分片分析摘要",
        "",
        "| Identification | 分片数量 | Frames | 完整性 |",
        "|---|---:|---|---|",
        *(fragment_table or ["| 缺失 | 0 |  | FAIL |"]),
        "",
        "## 5. TCP 分析摘要",
        "",
        "| tcp.stream | Client | Server | 握手 | 正常释放 |",
        "|---:|---|---|---|---|",
        *(tcp_table or ["| 缺失 |  |  | FAIL | FAIL |"]),
        "",
        "## 6. 验收结果和重抓建议",
        "",
    ]
    for name, result in results.items():
        lines.append(f"- {name}: **{result.status}** - {result.detail}")

    lines.extend(
        [
            "",
            "> 本汇总仅依据现有 pcap/CSV。缺失或不完整的数据必须重抓，"
            "不得用理论值或示例值替代。",
            "",
        ]
    )
    has_verified_source = any(
        (EXPORTS_DIR / csv_name).is_file()
        and (CAPTURES_DIR / capture_name).is_file()
        for csv_name, capture_name in (
            ("dhcp_fields.csv", "dhcp.pcapng"),
            ("icmp_fields.csv", "icmp.pcapng"),
            ("ip_fragment_fields.csv", "ip_fragment.pcapng"),
            ("tcp_fields.csv", "tcp.pcapng"),
        )
    )
    if has_verified_source:
        SUMMARY_PATH.write_text("\n".join(lines), encoding="utf-8")

    for name, result in results.items():
        print(f"[{result.status}] {name}: {result.detail}")
    if has_verified_source:
        print(f"[INFO] Summary written to {SUMMARY_PATH}")
    else:
        print("[INFO] No verified capture/CSV pair; summary was not created.")

    return 0 if all(result.status == "PASS" for result in results.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
