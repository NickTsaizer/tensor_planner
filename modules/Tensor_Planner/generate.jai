#run,stallable {
    set_build_options_dc(.{do_output=false});
    if !generate_tensor_planner_bindings() {
        compiler_set_workspace_status(.FAILED);
    }
}

generate_tensor_planner_bindings :: () -> bool {
    options:         Generate_Bindings_Options;
    output_filename: string;

    #if OS == .LINUX {
        output_filename = "generated/linux.jai";
        array_add(*options.libraries, .{filename="libtensor_planner"});
    } else #if OS == .WINDOWS {
        output_filename = "generated/windows.jai";
        array_add(*options.libraries, .{filename="tensor_planner"});
    } else #if OS == .MACOS {
        output_filename = "generated/macos.jai";
        array_add(*options.libraries, .{filename="libtensor_planner"});
    } else {
        log("Unsupported OS for Tensor_Planner binding generation.");
        return false;
    }

    array_add(*options.source_files, "../../include/tensor_planner.h");
    array_add(*options.include_paths, "../../include");
    array_add(*options.extra_clang_arguments, "-x", "c", "-std=c11");
    options.log_unsupported                     = true;
    options.generate_compile_time_struct_checks = true;

    return generate_bindings(options, output_filename);
}

#scope_file;

#import "Basic";
#import "Compiler";
#import "Bindings_Generator";
