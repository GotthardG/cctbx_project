
from dxtbx.model.experiment_list import DetectorComparison
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.ticker as tick


class Spotfinder_radial_average:

  def __init__(self, params):
    self.params = params
    n_panels = len(params.input.experiments[0].data[0].detector)
    self.panelsums = [np.zeros(params.n_bins)] * n_panels

  def _process_pixel(self, i_panel, s0, panel, xy, value):
    value -= self.params.downweight_weak
    d_max_inv = 1/self.params.d_max
    d_min_inv = 1/self.params.d_min
    res_inv = 1 / panel.get_resolution_at_pixel(s0, xy)
    n_bins = self.params.n_bins
    i_bin = int(
        n_bins * (res_inv - d_max_inv ) / (d_min_inv - d_max_inv)
        )
    if i_bin < 0 or i_bin >= n_bins: return
    self.panelsums[i_panel][i_bin] += value

  def calculate(self):
    params = self.params

    # TODO: flatten_xxxx
    assert len(params.input.reflections)==1, "Please supply 1 reflections file"
    assert len(params.input.experiments)==1, "Please supply 1 experiments file"

    # setup limits and bins
    n_bins = params.n_bins
    d_max, d_min = params.d_max, params.d_min
    d_inv_low, d_inv_high = 1/d_max, 1/d_min
    unit_wt = (params.peak_weighting == "unit")
    refls = params.input.reflections[0].data
    expts = params.input.experiments[0].data

    #apply beam center correction to expts
    detector = expts[0].detector
    if not np.allclose(params.xyz_offset, [0,0,0]):
      ref_detector = copy.deepcopy(detector)
      hierarchy = detector.hierarchy()
      fast = hierarchy.get_local_fast_axis()
      slow = hierarchy.get_local_slow_axis()
      origin = hierarchy.get_local_origin()
      corrected_origin = (
              origin[0] + params.xyz_offset[0],
              origin[1] + params.xyz_offset[1],
              origin[2] + params.xyz_offset[2]
              )
      hierarchy.set_local_frame(fast, slow, corrected_origin)
    else:
      ref_detector = detector
    compare_detector = DetectorComparison()
    for expt in expts:
      if expt.detector is detector: continue
      assert compare_detector(ref_detector, expt.detector)
      expt.detector = detector

    for i, expt in enumerate(expts):
      if i % 1000 == 0: print("experiment ", i)
      s0 = expt.beam.get_s0()
      sel = refls['id'] == i
      refls_sel = refls.select(sel)
      xyzobses = refls_sel['xyzobs.px.value']
      intensities = refls_sel['intensity.sum.value']
      panels = refls_sel['panel']
      shoeboxes = refls_sel['shoebox']

      for i_refl in range(len(refls_sel)):
        i_panel = panels[i_refl]
        panel = expt.detector[i_panel]
        
        peak_height = intensities[i_refl]
        if params.peak_position=="xyzobs":
          xy = xyzobses[i_refl][0:2]
          if params.peak_weighting == "intensity":
            value = intensities[i_refl]
          else:
            value = 1
          self._process_pixel(i_panel, s0, panel, xy, value)
        if params.peak_position=="shoebox":
          sb = shoeboxes[i_refl]
          sbpixels = zip(sb.coords(), sb.values())
          for (x,y,_), value in sbpixels:
            self._process_pixel(i_panel, s0, panel, (x,y), value)


  def plot(self):
    params = self.params
    d_max_inv = 1/params.d_max
    d_min_inv = 1/params.d_min
    xvalues = np.linspace(d_max_inv, d_min_inv, params.n_bins)
    fig, ax = plt.subplots()
    if params.split_panels:
      # TODO: better way to stack the split patterns
      offset = 0.5*max(np.array(self.panelsums[0]))
      for i_sums, sums in enumerate(self.panelsums):
        yvalues = np.array(sums)
        plt.plot(xvalues, yvalues+0.5*i_sums*offset)
    else:
      yvalues = sum(self.panelsums)
      plt.plot(xvalues, yvalues)
    ax.get_xaxis().set_major_formatter(tick.FuncFormatter(
      lambda x, _: "{:.3f}".format(1/x)))

    if params.output.xy_file:
      with open(params.output.xy_file, 'w') as f:
        for x,y in zip(xvalues, yvalues):
          f.write("{:.6f}\t{}\n".format(1/x, y))
    plt.show()

