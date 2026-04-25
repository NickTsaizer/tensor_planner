#scope_export;

status_name :: (status: TP_Status) -> string {
    if status == {
        case .OK;               return "OK";
        case .INVALID_ARGUMENT; return "INVALID_ARGUMENT";
        case .LIMIT_EXCEEDED;   return "LIMIT_EXCEEDED";
        case .NOT_FOUND;        return "NOT_FOUND";
        case .UNSUPPORTED;      return "UNSUPPORTED";
        case .NO_SOLUTION;      return "NO_SOLUTION";
    }

    return "UNKNOWN";
}

assert_ok :: (status: TP_Status, operation: string) {
    if status != .OK {
        log("Tensor Planner error in %: %", operation, status_name(status));
        assert(false);
    }
}
