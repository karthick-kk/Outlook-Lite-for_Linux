use once_cell::sync::Lazy;
use std::collections::HashSet;

static BLOCKED: Lazy<HashSet<&str>> = Lazy::new(|| {
    HashSet::from([
        "adsdkprod.azureedge.net",
        "adsdk.microsoft.com",
        "ads.microsoft.com",
        "doubleclick.net",
        "googlesyndication.com",
        "googleadservices.com",
        "google-analytics.com",
        "googletagmanager.com",
        "googletagservices.com",
        "adservice.google.com",
        "pagead2.googlesyndication.com",
        "bat.bing.com",
        "c.bing.com",
        "c.msn.com",
        "a-ring-fallback.msedge.net",
        "fp.msedge.net",
        "ntp.msn.com",
        "assets.msn.com",
        "outlookads.live.com",
        "ads.prod.outlookads.live.com",
        "dsp.prod.outlookads.live.com",
        "native.prod.outlookads.live.com",
        "browser.events.data.msn.com",
        "self.events.data.microsoft.com",
        "mobile.events.data.microsoft.com",
        "connect.facebook.net",
        "pixel.facebook.com",
        "amazon-adsystem.com",
        "criteo.com",
        "criteo.net",
        "taboola.com",
        "outbrain.com",
        "scorecardresearch.com",
        "quantserve.com",
        "adsymptotic.com",
        "adnxs.com",
        "rubiconproject.com",
    ])
});

/// Returns true if the URL matches a blocked ad/tracker domain.
pub fn is_blocked(url: &str) -> bool {
    for domain in BLOCKED.iter() {
        if url.contains(domain) {
            return true;
        }
    }
    false
}
