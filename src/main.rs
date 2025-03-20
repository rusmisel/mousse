use wayland_client::{
    protocol::{wl_display, wl_keyboard, wl_output, wl_registry},
    Connection, Dispatch, QueueHandle,
};
use wayland_protocols_wlr::{
    layer_shell::v1::client::zwlr_layer_shell_v1,
    virtual_pointer::v1::client::zwlr_virtual_pointer_v1,
};

enum Mousse {
    SearchingOutput,
    Selecting {
        w: i32,
        h: i32,
        output: wl_output::WlOutput,
    },
}

fn main() -> anyhow::Result<()> {
    let con = Connection::connect_to_env()?;
    let mut q = con.new_event_queue();
    let mut state = Mousse::SearchingOutput;
    con.display().get_registry(&q.handle(), ());
    loop {
        q.blocking_dispatch(&mut state)?;
    }
}

impl Dispatch<wl_registry::WlRegistry, ()> for Mousse {
    fn event(
        _st8: &mut Self,
        reg: &wl_registry::WlRegistry,
        evt: wl_registry::Event,
        _: &(),
        _con: &Connection,
        q: &QueueHandle<Self>,
    ) {
        if let wl_registry::Event::Global {
            name,
            interface,
            version,
        } = evt
        {
            match &interface[..] {
                "wl_output" => {
                    reg.bind::<wl_output::WlOutput, _, _>(name, version, q, ());
                }
                _ => {}
            }
        }
    }
}

impl Dispatch<wl_output::WlOutput, ()> for Mousse {
    fn event(
        _st8: &mut Self,
        _out: &wl_output::WlOutput,
        evt: wl_output::Event,
        _: &(),
        _con: &Connection,
        _q: &QueueHandle<Self>,
    ) {
        match evt {
            wl_output::Event::Mode {
                flags,
                width,
                height,
                refresh,
            } => {
                if let wl_output::Mode::Current = flags.into_result().unwrap() {
                    println!("current output: {width}x{height}@{refresh}");
                }
            }
            _ => {}
        }
    }
}

impl Dispatch<wl_keyboard::WlKeyboard, ()> for Mousse {
    fn event(
        _st8: &mut Self,
        _: &wl_keyboard::WlKeyboard,
        evt: wl_keyboard::Event,
        _: &(),
        _con: &Connection,
        _q: &QueueHandle<Self>,
    ) {
        println!("{evt:?}");
    }
}

impl Dispatch<zwlr_virtual_pointer_v1::ZwlrVirtualPointerV1, ()> for Mousse {
    fn event(
        _st8: &mut Self,
        _: &zwlr_virtual_pointer_v1::ZwlrVirtualPointerV1,
        evt: zwlr_virtual_pointer_v1::Event,
        _: &(),
        _con: &Connection,
        _q: &QueueHandle<Self>,
    ) {
        println!("{evt:?}");
    }
}
