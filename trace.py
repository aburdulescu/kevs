def trace_begin():
    pass


def trace_end():
    pass


def trace_unhandled(event_name, context, event_fields_dict, perf_sample_dict):
    s = event_fields_dict["common_s"]
    ns = event_fields_dict["common_ns"]
    timestamp = s * 1e9 + ns
    print("%u %s" % (timestamp, event_name))
