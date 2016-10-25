/* Copyright 2016 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
import {ProjectionsPanel} from './vz-projector-projections-panel';

const assert = chai.assert;

describe('setPCAComponentUIValues', () => {
  it('sets the pcaX/Y properties when setting 2D component values', () => {
    let projectionsPanel = document.createElement(
        ProjectionsPanel.prototype.is) as ProjectionsPanel;

    spyOn(projectionsPanel, 'setZDropdownEnabled');

    projectionsPanel.setPCAComponentUIValues([0, 1]);

    assert.equal(0, projectionsPanel.pcaX);
    assert.equal(1, projectionsPanel.pcaY);

    expect(projectionsPanel.setZDropdownEnabled).toHaveBeenCalledWith(false);
  });

  it('sets the pcaX/Y properties when setting 3D component values', () => {
    let projectionsPanel = document.createElement(
        ProjectionsPanel.prototype.is) as ProjectionsPanel;

    spyOn(projectionsPanel, 'setZDropdownEnabled');

    projectionsPanel.setPCAComponentUIValues([0, 1, 2]);

    assert.equal(0, projectionsPanel.pcaX);
    assert.equal(1, projectionsPanel.pcaY);
    assert.equal(2, projectionsPanel.pcaZ);

    expect(projectionsPanel.setZDropdownEnabled).toHaveBeenCalledWith(true);
  });
});

describe('getPCAComponentUIValues', () => {
  it('gets the PCA component UI values from a 2D PCA projection', () => {
    let projectionsPanel = document.createElement(
        ProjectionsPanel.prototype.is) as ProjectionsPanel;

    projectionsPanel.pcaX = 0;
    projectionsPanel.pcaY = 1;
    projectionsPanel.is3d = false;

    assert.deepEqual([0, 1], projectionsPanel.getPCAComponentUIValues());
  });

  it('gets the PCA component UI values from a 3D PCA projection', () => {
    let projectionsPanel = document.createElement(
        ProjectionsPanel.prototype.is) as ProjectionsPanel;

    projectionsPanel.pcaX = 0;
    projectionsPanel.pcaY = 1;
    projectionsPanel.pcaZ = 2;
    projectionsPanel.is3d = true;

    assert.deepEqual([0, 1, 2], projectionsPanel.getPCAComponentUIValues());
  });
});
