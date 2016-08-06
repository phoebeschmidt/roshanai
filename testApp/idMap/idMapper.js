var relayAddressRegExp = new RegExp("^[1-8]{3}$");
var serverAddress = "http://localhost:8666/map/ids"

$(document).ready(function() {
  setButtonClickEvents();

});

function setButtonClickEvents() {
  $('.solonoid-go-button').click(function() {
    console.log("CLICKED")
    var solonoid = $(this).parent().parent().parent();
    var id = solonoid.attr("solonoid-id");
    var val = solonoid.find("input").val();
    if (relayAddressRegExp.test(val)) {
      selectSolonoidLogo(id);
      solonoid.find(".error-message").hide();
      mapAddress(id, val);
    } else {
      solonoid.find(".error-message").show();
    }
  });

  $('.solonoid-heart-button').click(function() {
    var solonoid = $(this).parent().parent().parent();
    var id = solonoid.attr('solonoid-id');
    selectSolonoidLogo(id);
  });

}
function selectSolonoidLogo(id) {
  var selectorStr = ".small-img#" + id;
  $(selectorStr).css("color", "red");
  setTimeout(function() {
    $(selectorStr).css("color", "black");
  }, 500);
}

function mapAddress(id, address) {
  var data = {
    solonoidId: id,
    relayAddress: address
  }
  console.log("about to make post request");
  $.post(serverAddress, JSON.stringify(data), function(data, status) {
    console.log(data, status);
    setButtonClickEvents();
  });
}
