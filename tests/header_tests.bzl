"""Public-header self-containment test targets."""

load("@rules_cc//cc:defs.bzl", "cc_test")


def flight_cpp_public_header_tests():
    """Declares one isolated compilation and execution test per C++ header."""
    tests = []
    for name, selector in _PUBLIC_HEADERS:
        target = "header_{}_test".format(name)
        cc_test(
            name = target,
            srcs = ["header_self_containment_test.cpp"],
            local_defines = ["FLIGHT_CPP_HEADER_SELECTOR={}".format(selector)],
            size = "small",
            deps = ["//:cpp"],
        )
        tests.append(":" + target)

    native.test_suite(
        name = "public_header_tests",
        tests = tests,
    )


_PUBLIC_HEADERS = [
    ("array", 1),
    ("contract", 2),
    ("date", 3),
    ("equality", 4),
    ("error", 5),
    ("executor", 6),
    ("host", 7),
    ("map", 8),
    ("math", 9),
    ("presence", 10),
    ("rejection", 11),
    ("runtime", 12),
    ("set", 13),
    ("string", 14),
    ("task", 15),
    ("typed_array", 16),
    ("version", 17),
    ("reference", 18),
    ("number", 19),
    ("object", 20),
    ("symbol", 21),
    ("text_decoder", 22),
    ("url", 23),
    ("array_buffer", 24),
    ("data_view", 25),
    ("regexp", 26),
    ("intl", 27),
    ("callable", 28),
    ("json", 29),
    ("structural_ref", 30),
    ("weak_map", 31),
]
