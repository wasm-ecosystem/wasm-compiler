const child_process = require("child_process");
const { exit, env } = require("process");

const port = 12345;
const binPath = "build_fuzz/bin/vb_debugger_fuzz";

child_process.execSync("python --version", { stdio: "inherit" });
child_process.execSync("qemu-system-tricore --version", { stdio: "inherit" });
child_process.execSync("tricore-elf-gdb --version", { stdio: "inherit" });

child_process.spawn(
  "qemu-system-tricore",
  ["-semihosting", "-display", "none", "-M", "tricore_tsim162", "-kernel", binPath, "-gdb", `tcp::${port}`, "-S"],
  { env, stdio: ["inherit", "inherit", "inherit"], shell: true }
);
let gdb = child_process.spawn(
  "tricore-elf-gdb",
  [binPath, "--quiet", "-x", "./fuzz/fuzz_with_debugger/fuzz_init.gdb", `-ex`, `"start_fuzz ${port}"`],
  {
    env,
    stdio: ["inherit", "inherit", "inherit"],
    shell: true,
  }
);

var isTimeout = false;

gdb.on("exit", () => {
  process.exit(isTimeout ? 0 : -1);
});

setTimeout(() => {
  isTimeout = true;
  console.log("finish fuzz due to timeout");
  child_process.execSync("pkill qemu");
  exit(0);
}, env["TIMEOUT"] * 1000);
