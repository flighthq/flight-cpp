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
    ("erased_ref", 5),
    ("error", 6),
    ("executor", 7),
    ("font_face", 8),
    ("host", 9),
    ("map", 10),
    ("math", 11),
    ("presence", 12),
    ("rejection", 13),
    ("runtime", 14),
    ("set", 15),
    ("string", 16),
    ("task", 17),
    ("typed_array", 18),
    ("version", 19),
    ("reference", 20),
    ("number", 21),
    ("object", 22),
    ("symbol", 23),
    ("text_decoder", 24),
    ("url", 25),
    ("array_buffer", 26),
    ("data_view", 27),
    ("regexp", 28),
    ("intl", 29),
    ("callable", 30),
    ("conditional_facet_ref", 31),
    ("json", 32),
    ("structural_ref", 33),
    ("weak_map", 34),
    ("array_buffer_view", 35),
    ("sequence_view", 36),
    ("record", 37),
    ("weak_set", 38),
    ("abort", 39),
    ("blob", 40),
    ("uri", 41),
    ("base64", 42),
    ("stream", 43),
    ("text_encoder", 44),
    ("audio_buffer", 45),
    ("iterable", 46),
    ("web_types", 47),
    ("boolean", 48),
    ("iterator", 49),
    ("dom_exception", 50),
    ("image_data", 51),
    ("canvas_2d", 52),
    ("any", 53),
    ("structured_clone", 54),
    ("attachment", 55),
]
