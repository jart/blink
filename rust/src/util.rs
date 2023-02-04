use lazy_static::lazy_static;
use std::sync::Mutex;

#[no_mangle]
#[allow(non_snake_case)]
pub extern "C" fn endswith(s: *const libc::c_char, suffix: *const libc::c_char) -> bool {
    unsafe {
        let n = libc::strlen(s);
        let m = libc::strlen(suffix);
        return match m > n {
            true => false,
            false => libc::memcmp(s.offset((n-m) as _) as _, suffix as _, m) == 0
        }
    }
}

#[no_mangle]
#[allow(non_snake_case)]
pub extern "C" fn IsProcessTainted() -> u64 {
    if cfg!(target_os = "linux") {
        unsafe {
            libc::getauxval(libc::AT_SECURE)
        }
    } else {
        // TODO: return issetugid for BSDs.
        0
    }
}

/// Returns the number of logical cores and caches the result
#[no_mangle]
#[allow(non_snake_case)]
pub extern "C" fn GetCpuCount() -> i32 {
    lazy_static! {
        static ref LAST_VAL: Mutex<Option<i32>> = Mutex::new(None);
    }
    let mut last = LAST_VAL.lock().unwrap();
    let r = last.and_then(|n| {
        Some(n)
    }).or_else(|| Some(num_cpus::get() as i32));
    let n = r.unwrap();
    *last = r;
    n
}
