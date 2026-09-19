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
    ("font_face", 7),
    ("host", 8),
    ("map", 9),
    ("math", 10),
    ("presence", 11),
    ("rejection", 12),
    ("runtime", 13),
    ("set", 14),
    ("string", 15),
    ("task", 16),
    ("typed_array", 17),
    ("version", 18),
    ("reference", 19),
    ("number", 20),
    ("object", 21),
    ("symbol", 22),
    ("text_decoder", 23),
    ("url", 24),
    ("array_buffer", 25),
    ("data_view", 26),
    ("regexp", 27),
    ("intl", 28),
    ("callable", 29),
    ("conditional_facet_ref", 30),
    ("json", 31),
    ("structural_ref", 32),
    ("weak_map", 33),
    ("array_buffer_view", 34),
    ("sequence_view", 35),
    ("record", 36),
    ("weak_set", 37),
    ("abort", 38),
    ("blob", 39),
    ("uri", 40),
    ("base64", 41),
    ("stream", 42),
    ("text_encoder", 43),
    ("audio_buffer", 44),
    ("iterable", 45),
    ("web_types", 46),
    ("boolean", 47),
    ("iterator", 48),
    ("dom_exception", 49),
    ("image_data", 50),
    ("canvas_2d", 51),
    ("any", 52),
    ("structured_clone", 53),
    ("attachment", 54),
]
