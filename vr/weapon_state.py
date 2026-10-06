"""Native gameplay preparation for isolated weapon diagnostics."""


def resume_if_paused(command, wait_frames):
    # Existing measured native pause flag; resume through Start, never a RAM edit.
    def paused():
        return int.from_bytes(bytes.fromhex(
            command('read_ram', addr='0x8009a61c', len=4)['hex']), 'little') == 1
    if not paused():
        return False
    command('press', buttons=0xffff ^ 0x0008, frames=4)
    wait_frames(16)
    command('clear_input')
    if paused():
        raise RuntimeError('Native Start did not resume the paused weapon save')
    return True
