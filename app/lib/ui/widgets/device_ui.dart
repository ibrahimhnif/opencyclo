import 'package:flutter/material.dart';
import '../theme/app_theme.dart';

/// The OpenCyclo device component kit.
///
/// Each widget here is a deliberate port of one firmware render function, so a
/// screen built from these reads like the device screen it mirrors. The mapping:
///
///   DeviceStatusBar  <- renderPage()            status header, 3 segments
///   DeviceTile       <- renderTile()            hairline + label + value
///   DeviceHeroTile   <- renderWidgetSpeed()     the one oversized readout
///   DeviceListRow    <- renderWidgetSettingsList() row lambda
///   DeviceButton     <- the action / scan buttons (fillRoundRect + black text)
///   DeviceChip       <- the "forget" button on a sensor row
///
/// The page indicator dots the firmware draws at y = 312 live in DevicePreview,
/// which renders a whole device screen; nothing else in the app needs them.
///
/// House rules, inherited from the firmware and enforced by these widgets:
///   * Every string is lowercase. `DeviceText.normalise` applies it so a caller
///     can't accidentally shout.
///   * Structure is a top hairline, never a border or a fill.
///   * Units belong in the label, not the value.

// ---------------------------------------------------------------------------
// Text normalisation
// ---------------------------------------------------------------------------

class DeviceText {
  const DeviceText._();

  /// The device draws every string lowercase. Rather than trusting call sites,
  /// funnel display strings through here. Values are exempt — a mac address or
  /// a firmware version keeps its own casing.
  static String normalise(String s) => s.toLowerCase();
}

// ---------------------------------------------------------------------------
// Hairline — drawFastHLine(b.x, b.y, b.w, COLOR_HAIRLINE)
// ---------------------------------------------------------------------------

class DeviceHairline extends StatelessWidget {
  final double indent;
  final double endIndent;

  const DeviceHairline({super.key, this.indent = 0, this.endIndent = 0});

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: EdgeInsets.only(left: indent, right: endIndent),
      child: Container(
        height: AppTheme.hairlineWidth,
        color: AppTheme.hairline,
      ),
    );
  }
}

// ---------------------------------------------------------------------------
// Status bar — renderPage() segments 1-3
//
// The device row is: page title (muted) | gps fix (green/amber) | ride state +
// battery (green/amber/muted). The app maps the middle segment to its own live
// signal — the BLE link — because that is this screen's equivalent truth.
// ---------------------------------------------------------------------------

class DeviceStatusBar extends StatelessWidget implements PreferredSizeWidget {
  /// Segment 1 — the screen name. Drawn muted, exactly like `page.title`.
  final String title;

  /// Segment 2 — the live link indicator.
  final String linkText;
  final Color linkColor;

  /// Segment 3 — trailing state. Omitted when the device isn't reporting.
  final String? stateText;
  final Color? stateColor;

  /// Optional trailing action, e.g. the layout builder's "read from device".
  final Widget? action;

  const DeviceStatusBar({
    super.key,
    required this.title,
    required this.linkText,
    required this.linkColor,
    this.stateText,
    this.stateColor,
    this.action,
  });

  @override
  Size get preferredSize => const Size.fromHeight(52);

  @override
  Widget build(BuildContext context) {
    return SafeArea(
      bottom: false,
      child: SizedBox(
        height: 52,
        child: Column(
          children: [
            Expanded(
              child: Padding(
                padding: const EdgeInsets.symmetric(
                  horizontal: AppTheme.gutter,
                ),
                child: Row(
                  children: [
                    // Segment 1: title.
                    Text(
                      DeviceText.normalise(title),
                      style: AppTheme.labelStyle,
                    ),
                    const Spacer(),
                    // Segment 2: link status.
                    Text(
                      DeviceText.normalise(linkText),
                      style: AppTheme.statusStyle(linkColor),
                    ),
                    // Segment 3: ride state + battery.
                    if (stateText != null) ...[
                      const SizedBox(width: 12),
                      Text(
                        DeviceText.normalise(stateText!),
                        style: AppTheme.statusStyle(
                          stateColor ?? AppTheme.label,
                        ),
                      ),
                    ],
                    if (action != null) ...[
                      const SizedBox(width: 4),
                      action!,
                    ],
                  ],
                ),
              ),
            ),
            const DeviceHairline(),
          ],
        ),
      ),
    );
  }
}

// ---------------------------------------------------------------------------
// Tile — renderTile()
//
// Firmware: hairline across the top edge, label at +4 in COLOR_LABEL, value at
// +24 in the widget's colour. No fill, no border, no radius.
// ---------------------------------------------------------------------------

class DeviceTile extends StatelessWidget {
  final String label;
  final String value;
  final Color valueColor;

  /// Shown small and muted after the value. Prefer putting the unit in the
  /// label ("dist (km)"), matching the firmware; this is for the rare case
  /// where the unit genuinely belongs beside the number.
  final String? trailingUnit;

  final VoidCallback? onTap;

  const DeviceTile({
    super.key,
    required this.label,
    required this.value,
    this.valueColor = AppTheme.text,
    this.trailingUnit,
    this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    final content = Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      mainAxisSize: MainAxisSize.min,
      children: [
        const DeviceHairline(),
        const SizedBox(height: 10),
        Text(
          DeviceText.normalise(label),
          style: AppTheme.labelStyle,
          maxLines: 1,
          overflow: TextOverflow.ellipsis,
        ),
        const SizedBox(height: 8),
        Row(
          crossAxisAlignment: CrossAxisAlignment.baseline,
          textBaseline: TextBaseline.alphabetic,
          children: [
            Flexible(
              child: Text(
                value,
                style: AppTheme.valueStyle(valueColor),
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
              ),
            ),
            if (trailingUnit != null) ...[
              const SizedBox(width: 5),
              Text(
                DeviceText.normalise(trailingUnit!),
                style: AppTheme.labelStyle,
              ),
            ],
          ],
        ),
        const SizedBox(height: 14),
      ],
    );

    if (onTap == null) return content;
    return GestureDetector(
      behavior: HitTestBehavior.opaque,
      onTap: onTap,
      child: content,
    );
  }
}

// ---------------------------------------------------------------------------
// Hero tile — renderWidgetSpeed()
//
// Firmware: "speed . gps" source label at +4, the 24pt value at +24, and the
// unit parked at the bottom-right corner of the tile.
// ---------------------------------------------------------------------------

class DeviceHeroTile extends StatelessWidget {
  final String label;
  final String value;
  final String unit;

  const DeviceHeroTile({
    super.key,
    required this.label,
    required this.value,
    required this.unit,
  });

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      mainAxisSize: MainAxisSize.min,
      children: [
        const DeviceHairline(),
        const SizedBox(height: 10),
        Text(DeviceText.normalise(label), style: AppTheme.labelStyle),
        const SizedBox(height: 4),
        Row(
          crossAxisAlignment: CrossAxisAlignment.baseline,
          textBaseline: TextBaseline.alphabetic,
          children: [
            Text(value, style: AppTheme.heroStyle),
            const Spacer(),
            Padding(
              padding: const EdgeInsets.only(bottom: 6),
              child: Text(
                DeviceText.normalise(unit),
                style: AppTheme.labelStyle,
              ),
            ),
          ],
        ),
        const SizedBox(height: 14),
      ],
    );
  }
}

// ---------------------------------------------------------------------------
// List row — renderWidgetSettingsList() row lambda
//
// Firmware: label left in COLOR_LABEL, value right in its own colour, hairline
// divider below. Also the shape of a BLE sensor row.
// ---------------------------------------------------------------------------

class DeviceListRow extends StatelessWidget {
  final String label;

  /// Right-hand value. Not lowercased — mac addresses and versions keep casing.
  final String? value;
  final Color valueColor;

  /// Optional second line under the label, e.g. "aa:bb:cc | rssi -62 dbm".
  final String? detail;

  /// Optional trailing control, e.g. a [DeviceChip] or a switch.
  final Widget? trailing;

  final VoidCallback? onTap;

  /// Suppresses the divider for the last row in a group.
  final bool showDivider;

  const DeviceListRow({
    super.key,
    required this.label,
    this.value,
    this.valueColor = AppTheme.text,
    this.detail,
    this.trailing,
    this.onTap,
    this.showDivider = true,
  });

  @override
  Widget build(BuildContext context) {
    final row = Padding(
      padding: const EdgeInsets.symmetric(vertical: 14),
      child: Row(
        children: [
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              mainAxisSize: MainAxisSize.min,
              children: [
                Text(
                  DeviceText.normalise(label),
                  style: AppTheme.labelStyle,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                ),
                if (detail != null) ...[
                  const SizedBox(height: 4),
                  Text(
                    detail!,
                    style: AppTheme.detailStyle,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                  ),
                ],
              ],
            ),
          ),
          if (value != null) ...[
            const SizedBox(width: 12),
            Flexible(
              child: Text(
                value!,
                style: AppTheme.valueStyle(valueColor).copyWith(fontSize: 14),
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                textAlign: TextAlign.right,
              ),
            ),
          ],
          if (trailing != null) ...[
            const SizedBox(width: 12),
            trailing!,
          ],
        ],
      ),
    );

    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        if (onTap != null)
          GestureDetector(
            behavior: HitTestBehavior.opaque,
            onTap: onTap,
            child: row,
          )
        else
          row,
        if (showDivider) const DeviceHairline(),
      ],
    );
  }
}

// ---------------------------------------------------------------------------
// Button — the action / scan buttons
//
// Firmware: fillRoundRect in a solid status colour with BLACK text on top. The
// device never draws an outlined or ghost button; colour carries the meaning.
// ---------------------------------------------------------------------------

class DeviceButton extends StatelessWidget {
  final String text;
  final Color color;
  final VoidCallback? onPressed;
  final IconData? icon;

  /// Swaps the icon for a spinner and blocks the tap, mirroring the device's
  /// "scanning..." state on its own scan button.
  final bool busy;

  /// Outlined variant for secondary choices the device has no equivalent for
  /// (file pickers and the like). Uses the same radius and lowercase label.
  final bool outlined;

  const DeviceButton({
    super.key,
    required this.text,
    required this.color,
    this.onPressed,
    this.icon,
    this.busy = false,
    this.outlined = false,
  });

  @override
  Widget build(BuildContext context) {
    final enabled = onPressed != null && !busy;
    final effective = enabled ? color : AppTheme.hairline;
    final foreground = outlined
        ? (enabled ? color : AppTheme.label)
        : (enabled ? AppTheme.bg : AppTheme.label);

    return GestureDetector(
      onTap: enabled ? onPressed : null,
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 120),
        height: 52,
        decoration: BoxDecoration(
          color: outlined ? Colors.transparent : effective,
          borderRadius: BorderRadius.circular(AppTheme.radiusAction),
          border: outlined
              ? Border.all(color: effective, width: AppTheme.hairlineWidth)
              : null,
        ),
        alignment: Alignment.center,
        child: Row(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            if (busy)
              SizedBox(
                width: 14,
                height: 14,
                child: CircularProgressIndicator(
                  strokeWidth: 2,
                  valueColor: AlwaysStoppedAnimation<Color>(foreground),
                ),
              )
            else if (icon != null)
              Icon(icon, size: 16, color: foreground),
            if (busy || icon != null) const SizedBox(width: 10),
            Flexible(
              child: Text(
                DeviceText.normalise(text),
                style: AppTheme.statusStyle(foreground).copyWith(fontSize: 13),
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
              ),
            ),
          ],
        ),
      ),
    );
  }
}

// ---------------------------------------------------------------------------
// Chip — the "forget" button on a BLE sensor row
//
// Firmware: fillRoundRect(..., 52, 18, 4, COLOR_RED) with white text.
// ---------------------------------------------------------------------------

class DeviceChip extends StatelessWidget {
  final String text;
  final Color color;
  final VoidCallback? onTap;

  const DeviceChip({
    super.key,
    required this.text,
    required this.color,
    this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTap: onTap,
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 7),
        decoration: BoxDecoration(
          color: color,
          borderRadius: BorderRadius.circular(AppTheme.radiusChip),
        ),
        child: Text(
          DeviceText.normalise(text),
          style: AppTheme.statusStyle(AppTheme.bg).copyWith(fontSize: 11),
        ),
      ),
    );
  }
}

// ---------------------------------------------------------------------------
// Section label — the device's own way of titling a block of rows
// ---------------------------------------------------------------------------

class DeviceSectionLabel extends StatelessWidget {
  final String text;
  final Widget? trailing;

  const DeviceSectionLabel({super.key, required this.text, this.trailing});

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 10, top: 8),
      child: Row(
        children: [
          Text(DeviceText.normalise(text), style: AppTheme.labelStyle),
          if (trailing != null) ...[const Spacer(), trailing!],
        ],
      ),
    );
  }
}

// ---------------------------------------------------------------------------
// Empty state — the device's "not paired" / "gps: searching..." voice
// ---------------------------------------------------------------------------

class DeviceEmptyState extends StatelessWidget {
  final String text;

  const DeviceEmptyState({super.key, required this.text});

  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.symmetric(vertical: 32),
      alignment: Alignment.center,
      child: Text(
        DeviceText.normalise(text),
        style: AppTheme.labelStyle,
        textAlign: TextAlign.center,
      ),
    );
  }
}

// ---------------------------------------------------------------------------
// Screen scaffold — black canvas, status bar, consistent gutters
// ---------------------------------------------------------------------------

class DeviceScreen extends StatelessWidget {
  final DeviceStatusBar statusBar;
  final Widget body;

  /// Pinned to the bottom above the nav bar, like the device's action button.
  final Widget? bottomAction;

  const DeviceScreen({
    super.key,
    required this.statusBar,
    required this.body,
    this.bottomAction,
  });

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppTheme.bg,
      appBar: statusBar,
      body: body,
      bottomNavigationBar: bottomAction == null
          ? null
          : SafeArea(
              top: false,
              child: Padding(
                padding: const EdgeInsets.fromLTRB(
                  AppTheme.gutter,
                  8,
                  AppTheme.gutter,
                  12,
                ),
                child: bottomAction,
              ),
            ),
    );
  }
}
