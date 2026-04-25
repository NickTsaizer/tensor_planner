#import "Basic";

#scope_export;

#load "platform.jai";

#if OS == .LINUX {
    #load "generated/linux.jai";
} else #if OS == .WINDOWS {
    #load "generated/windows.jai";
} else #if OS == .MACOS {
    #load "generated/macos.jai";
}

#load "status.jai";
#load "schema_spec.jai";
#load "runtime.jai";
